/*
sessiond - standalone X session manager
Copyright (C) 2018 James Reed

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

#include "dbus-systemd.h"
#include "common.h"
#include "config.h"

#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define SYSTEMD_NAME "org.freedesktop.systemd1"
#define SYSTEMD_MANAGER_IFACE SYSTEMD_NAME ".Manager"
#define SYSTEMD_PATH "/org/freedesktop/systemd1"

static void
systemd_on_appear(GDBusConnection *conn, const gchar *name, const gchar *owner,
                  gpointer user_data)
{
    g_debug("%s appeared (owned by %s)", name, owner);

    SystemdContext *c = (SystemdContext *)user_data;
    GError *err = NULL;

    c->systemd_manager = g_dbus_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
        G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, NULL, SYSTEMD_NAME,
        SYSTEMD_PATH, SYSTEMD_MANAGER_IFACE, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    }
}

static void
systemd_on_vanish(UNUSED GDBusConnection *conn, const gchar *name,
                  gpointer user_data)
{
    g_debug("%s vanished", name);
    systemd_context_free((SystemdContext *)user_data);
}

void
systemd_start_unit(SystemdContext *c, const gchar *name)
{
    if (!c->systemd_manager) {
        g_warning("Cannot start unit %s: %s does not exist", name,
                  SYSTEMD_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(c->systemd_manager, "StartUnit",
                           g_variant_new("(ss)", name, "replace"),
                           G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("Started unit %s", name);
    }
}

SystemdContext *
systemd_context_new(void)
{
    SystemdContext *c = g_malloc0(sizeof(SystemdContext));

    c->systemd_watcher = g_bus_watch_name(
        G_BUS_TYPE_SESSION, SYSTEMD_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        systemd_on_appear, systemd_on_vanish, c, NULL);

    return c;
}

void
systemd_context_free(SystemdContext *c)
{
    if (!c)
        return;
    if (c->systemd_watcher) {
        g_bus_unwatch_name(c->systemd_watcher);
        c->systemd_watcher = 0;
    }
    if (c->systemd_manager) {
        g_object_unref(c->systemd_manager);
        c->systemd_manager = NULL;
    }
    g_free(c);
}
