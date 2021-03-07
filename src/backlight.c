/*
sessiond - standalone X session manager
Copyright (C) 2018-2020 James Reed

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#define G_LOG_DOMAIN "sessiond"

#include "backlight.h"
#include "common.h"
#include "config.h"
#include "dbus-logind.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib-2.0/glib.h>
#include <libudev.h>

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif /* PREFIX */

#define SYSFS_WRITER PREFIX "/lib/sessiond/sessiond-sysfs-writer"

static gchar *
get_sys_path(const char *subsystem, const char *name)
{
    return g_strjoin("/", "", "sys", "class", subsystem, name, NULL);
}

static gint32
get_uint_sysattr(struct Backlight *b, const char *attr)
{
    const char *str = udev_device_get_sysattr_value(b->device, attr);
    if (!str)
        return -1;
    gchar *end;
    errno = 0;
    guint64 i = g_ascii_strtoull(str, &end, 10);

    if (errno || end == str) {
        g_warning("Failed to parse backlight (%s) attr: %s", b->sys_path, attr);
        return -1;
    }

    return i;
}

static gboolean
update_backlight(struct Backlight *bl, struct udev_device *dev)
{
    if (bl->device)
        udev_device_unref(bl->device);
    if (!((bl->name = udev_device_get_sysname(dev)) &&
        (bl->subsystem = udev_device_get_subsystem(dev)) &&
        (bl->sys_path = get_sys_path(bl->subsystem, bl->name)) &&
        (bl->dev_path = udev_device_get_devpath(dev))))
        return FALSE;
    bl->device = udev_device_ref(dev);
    bl->brightness = get_uint_sysattr(bl, "brightness");
    bl->max_brightness = get_uint_sysattr(bl, "max_brightness");

    return TRUE;
}

static struct Backlight *
new_backlight(struct udev_device *dev)
{
    struct Backlight *bl = g_malloc(sizeof(struct Backlight));

    bl->device = NULL;
    bl->online = TRUE;
    bl->pre_dim_brightness = -1;
    if (!update_backlight(bl, dev)) {
        g_free(bl);
        return NULL;
    }

    return bl;
}

static void
free_backlight(struct Backlight *bl)
{
    udev_device_unref(bl->device);
    g_free(bl->sys_path);
    g_free(bl);
}

static BacklightAction
get_action(struct udev_device *dev)
{
    const char *action = udev_device_get_action(dev);

    if (!action)
        return BL_ACTION_ADD;

#define X(a, str) \
    if (g_strcmp0(action, str) == 0) \
        return BL_ACTION_##a;
    BL_ACTION_LIST
#undef X

    return BL_ACTION_ADD;
}

static gboolean
device_is_backlight(struct udev_device *dev)
{
    const char *sys = udev_device_get_subsystem(dev);
    const char *path = udev_device_get_syspath(dev);
    gboolean bl = g_strcmp0(sys, "backlight") == 0;
    return bl || g_str_has_suffix(path, "::kbd_backlight");
}

static gboolean
source_prepare(GSource *source, gint *timeout)
{
    Backlights *self = (Backlights *)source;
    *timeout = -1;
    return !g_queue_is_empty(self->queue);
}

static gboolean
source_check(GSource *source)
{
    Backlights *self = (Backlights *)source;
    GIOCondition revents = g_source_query_unix_fd(source, self->fd);

    if (revents & G_IO_IN) {
        struct udev_device *dev;
        while ((dev = udev_monitor_receive_device(self->udev_mon)))
            if (device_is_backlight(dev))
                g_queue_push_tail(self->queue, dev);
    }

    return !g_queue_is_empty(self->queue);
}

static gboolean
source_dispatch(GSource *source, GSourceFunc func, UNUSED gpointer user_data)
{
    Backlights *self = (Backlights *)source;
    struct udev_device *dev = g_queue_pop_head(self->queue);
    gchar *sys_path = get_sys_path(udev_device_get_subsystem(dev),
            udev_device_get_sysname(dev));
    BacklightAction action = get_action(dev);
    struct Backlight *bl = NULL;
    gboolean ret = TRUE;

    switch (action) {
        case BL_ACTION_ADD:
            bl = new_backlight(dev);
            if (!bl || !g_hash_table_insert(self->devices, (char *)sys_path, bl))
                goto end;
            break;
        case BL_ACTION_REMOVE:
            if (!g_hash_table_remove(self->devices, sys_path))
                goto end;
            break;
        case BL_ACTION_CHANGE:
            bl = g_hash_table_lookup(self->devices, sys_path);
            if (!bl)
                goto end;
            update_backlight(bl, dev);
            break;
        case BL_ACTION_ONLINE:
            bl = g_hash_table_lookup(self->devices, sys_path);
            if (!bl || bl->online)
                goto end;
            bl->online = TRUE;
            break;
        case BL_ACTION_OFFLINE:
            bl = g_hash_table_lookup(self->devices, sys_path);
            if (!bl || !bl->online)
                goto end;
            bl->online = FALSE;
            break;
    }

    ret = ((BacklightsFunc)func)(action, sys_path, bl);

end:
    g_free(sys_path);
    udev_device_unref(dev);
    return ret;
}

static void
source_finalize(GSource *source)
{
    Backlights *self = (Backlights *)source;

    g_queue_free_full(self->queue, (GDestroyNotify)udev_device_unref);
    g_hash_table_unref(self->devices);
    udev_monitor_unref(self->udev_mon);
    udev_unref(self->udev);
}

static GSourceFuncs source_funcs = {
    source_prepare,
    source_check,
    source_dispatch,
    source_finalize,
    NULL,
    NULL,
};

static void
backlights_init_devices(Backlights *self, BacklightsFunc func)
{
    struct udev_enumerate *e = udev_enumerate_new(self->udev);

    udev_enumerate_add_match_subsystem(e, "backlight");
    udev_enumerate_add_match_subsystem(e, "leds");
    udev_enumerate_scan_devices(e);

    struct udev_list_entry *devs = udev_enumerate_get_list_entry(e);
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, devs) {
        const char *name = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(self->udev,
                name);

        if (!device_is_backlight(dev))
            continue;

        struct Backlight *bl = new_backlight(dev);
        udev_device_unref(dev);

        if (g_hash_table_insert(self->devices, (char *)bl->sys_path, bl))
            func(BL_ACTION_ADD, bl->sys_path, bl);
    }

    udev_enumerate_unref(e);
}

static gboolean
set_backlight_brightness(struct Backlight *bl, guint32 v)
{
#ifndef BACKLIGHT_HELPER
    return FALSE;
#endif

    if (bl->max_brightness == -1)
        return FALSE;

    v = MIN(v, bl->max_brightness);

    gchar *str = g_strdup_printf("%u", v);
    gchar *brightness = g_strjoin("/", bl->sys_path, "brightness", NULL);
    gchar *argv[] = {SYSFS_WRITER, brightness, str, NULL};

    if (spawn_exec(argv) == 0)
        g_debug("Set %s brightness: %u", bl->sys_path, v);

    g_free(brightness);
    g_free(str);

    return TRUE;
}

static void
backlight_dim_value(struct Backlight *bl, guint32 v, LogindContext *ctx)
{
    bl->pre_dim_brightness = bl->brightness;
    backlight_set_brightness(bl, v, ctx);
}

static void
backlight_dim_percent(struct Backlight *bl, gdouble percent, LogindContext *ctx)
{
    gint32 v = bl->brightness;
    if (v == -1)
        return;
    bl->pre_dim_brightness = v;
    gdouble d = v - v * percent;
    backlight_set_brightness(bl, (guint32)(d > 0 ? d + 0.5 : d), ctx);
}

static void
backlight_restore(struct Backlight *bl, LogindContext *ctx)
{
    if (bl->pre_dim_brightness == -1)
        return;
    backlight_set_brightness(bl, bl->pre_dim_brightness, ctx);
    bl->pre_dim_brightness = -1;
}

Backlights *
backlights_new(GMainContext *ctx, BacklightsFunc func)
{
    GSource *source = g_source_new(&source_funcs, sizeof(Backlights));
    Backlights *self = (Backlights *)source;

    self->udev = udev_new();
    self->udev_mon = udev_monitor_new_from_netlink(self->udev, "udev");

    udev_monitor_filter_add_match_subsystem_devtype(self->udev_mon,
            "backlight", NULL);
    udev_monitor_filter_add_match_subsystem_devtype(self->udev_mon,
            "leds", NULL);
    udev_monitor_enable_receiving(self->udev_mon);

    self->queue = g_queue_new();
    self->devices = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
            (GDestroyNotify)free_backlight);

    backlights_init_devices(self, func);

    self->fd = g_source_add_unix_fd(source,
            udev_monitor_get_fd(self->udev_mon), G_IO_IN);

    g_source_set_callback(source, (GSourceFunc)func, NULL, NULL);
    g_source_attach(source, ctx);

    return self;
}

void
backlights_free(Backlights *bls)
{
    if (!bls)
        return;
    GSource *source = (GSource *)bls;
    g_source_destroy(source);
    g_source_unref(source);
}

void
backlights_restore(GHashTable *devs, LogindContext *ctx)
{
    GHashTableIter iter;
    gpointer bl;

    g_hash_table_iter_init(&iter, devs);
    while (g_hash_table_iter_next(&iter, NULL, &bl))
        backlight_restore(bl, ctx);
}

gchar *
backlight_normalize_name(const char *name)
{
    GRegex *regex = g_regex_new("::", 0, 0, NULL);
    gchar *norm = g_regex_replace(regex, name, -1, 0, "_", 0, NULL);
    g_regex_unref(regex);

    return norm;
}

gboolean
backlight_set_brightness(struct Backlight *bl, guint32 v, LogindContext *ctx)
{
    if (!logind_set_brightness(ctx, bl->subsystem, bl->name, v))
        return set_backlight_brightness(bl, v);

    return TRUE;
}

void
backlights_on_timeout(GHashTable *devs, GHashTable *cs, guint timeout,
        gboolean state, LogindContext *ctx)
{
    GHashTableIter iter;
    gpointer key, val;

    g_hash_table_iter_init(&iter, cs);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
        const char *sys_path = key;
        struct BacklightConf *c = val;

        if (c->dim_sec != timeout)
            continue;

        struct Backlight *bl = g_hash_table_lookup(devs, sys_path);

        if (!bl)
            continue;

        if (state) {
            if (c->dim_value != -1)
                backlight_dim_value(bl, c->dim_value, ctx);
            else
                backlight_dim_percent(bl, c->dim_percent, ctx);
        } else {
            backlight_restore(bl, ctx);
        }
    }
}
