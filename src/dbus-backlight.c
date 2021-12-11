/*
sessiond - standalone X session manager
Copyright (C) 2019-2020 James Reed

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

#include "dbus-backlight.h"
#include "dbus-server.h"
#include "dbus-gen.h"
#include "backlight.h"

#include <glib-2.0/glib.h>

static void
set_backlights_property(DBusServer *s)
{
    GList *dbls = g_hash_table_get_values(s->backlights);
    const gchar **paths = g_malloc0_n(g_list_length(dbls) + 1, sizeof(gchar *));

    guint n = 0;
    for (GList *i = dbls; i; i = i->next, n++) {
        DBusBacklight *dbl = i->data;
        const gchar *path = g_dbus_interface_skeleton_get_object_path(
                G_DBUS_INTERFACE_SKELETON(dbl));
        if (path)
            paths[n] = path;
    }

    dbus_session_set_backlights(s->session, paths);
    g_free(paths);
}

static gboolean
on_handle_set_brightness(DBusBacklight *dbl, GDBusMethodInvocation *i,
        guint32 v, gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;

    if (!s->bl_devices)
        return FALSE;

    const gchar *sys_path = dbus_backlight_get_sys_path(dbl);
    if (!sys_path)
        return FALSE;
    struct Backlight *bl = g_hash_table_lookup(s->bl_devices, sys_path);
    if (!bl)
        return FALSE;

    if (!backlight_set_brightness(bl, v, s->ctx)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_BACKLIGHT_ERROR ".SetBrightness",
                "Failed to set brightness");
        return TRUE;
    }

    dbus_backlight_complete_set_brightness(dbl, i);
    return TRUE;
}

static gboolean
on_handle_inc_brightness(DBusBacklight *dbl, GDBusMethodInvocation *i,
        gint v, gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;

    if (!s->bl_devices)
        return FALSE;

    const gchar *sys_path = dbus_backlight_get_sys_path(dbl);
    if (!sys_path)
        return FALSE;
    struct Backlight *bl = g_hash_table_lookup(s->bl_devices, sys_path);
    if (!bl)
        return FALSE;

    guint b = MAX(bl->brightness + v, 0);
    if (!backlight_set_brightness(bl, b, s->ctx)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_BACKLIGHT_ERROR ".IncBrightness",
                "Failed to increment brightness");
        return TRUE;
    }

    dbus_backlight_complete_inc_brightness(dbl, i, b);
    return TRUE;
}

static void
update_backlight(DBusBacklight *dbl, struct Backlight *bl)
{
    const gchar *str = NULL;
#define SET_STR(n) \
    str = dbus_backlight_get_##n(dbl); \
    if (!str || g_strcmp0(str, bl->n) != 0) \
        dbus_backlight_set_##n(dbl, bl->n)
    SET_STR(name);
    SET_STR(subsystem);
    SET_STR(sys_path);
    SET_STR(dev_path);
#undef SET_STR

#define SET_INT(n) \
    if (dbus_backlight_get_##n(dbl) != bl->n) \
        dbus_backlight_set_##n(dbl, bl->n)
    SET_INT(online);
    SET_INT(brightness);
    SET_INT(max_brightness);
#undef SET_INT
}

gboolean
dbus_server_export_backlight(DBusServer *s, DBusBacklight *dbl)
{
    const gchar *name = dbus_backlight_get_name(dbl);
    gchar *norm = backlight_normalize_name(name);
    gchar *path = g_strdup_printf("%s/%s", DBUS_BACKLIGHT_PATH,
            norm ? norm : name);

    if (norm)
        g_free(norm);

    GError *err = NULL;
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dbl), s->conn,
            path, &err);

    if (err) {
        g_error("Failed to export DBus Backlight interface: %s", err->message);
        g_error_free(err);
        g_free(path);
        return FALSE;
    }

    set_backlights_property(s);

    dbus_session_emit_add_backlight(s->session, path);
    g_free(path);

    return TRUE;
}

void
dbus_server_unexport_backlight(DBusServer *s, DBusBacklight *dbl)
{
    GDBusInterfaceSkeleton *skel = G_DBUS_INTERFACE_SKELETON(dbl);
    gchar *path = g_strdup(g_dbus_interface_skeleton_get_object_path(skel));

    g_dbus_interface_skeleton_unexport(skel);
    set_backlights_property(s);

    dbus_session_emit_remove_backlight(s->session, path);
    g_free(path);
}

void
dbus_server_add_backlight(DBusServer *s, struct Backlight *bl)
{
    DBusBacklight *dbl = dbus_backlight_skeleton_new();
    g_hash_table_insert(s->backlights, (char *)bl->sys_path, dbl);

    g_signal_connect(dbl, "handle-set-brightness",
            G_CALLBACK(on_handle_set_brightness), s);
    g_signal_connect(dbl, "handle-inc-brightness",
            G_CALLBACK(on_handle_inc_brightness), s);

    update_backlight(dbl, bl);

    if (s->name_acquired)
        dbus_server_export_backlight(s, dbl);
}

void
dbus_server_remove_backlight(DBusServer *s, const char *path)
{
    DBusBacklight *dbl = g_hash_table_lookup(s->backlights, path);

    if (!dbl)
        return;

    g_hash_table_steal(s->backlights, path);
    dbus_server_unexport_backlight(s, dbl);
    g_object_unref(dbl);
}

void
dbus_server_update_backlight(DBusServer *s, struct Backlight *bl)
{
    DBusBacklight *dbl = g_hash_table_lookup(s->backlights, bl->sys_path);

    if (dbl)
        update_backlight(dbl, bl);
}
