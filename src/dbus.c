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

#include "dbus.h"
#include "common.h"
#include "config.h"

#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define LOGIND_NAME "org.freedesktop.login1"
#define LOGIND_MANAGER_IFACE LOGIND_NAME ".Manager"
#define LOGIND_SESSION_IFACE LOGIND_NAME ".Session"
#define LOGIND_USER_IFACE LOGIND_NAME ".User"
#define LOGIND_PATH "/org/freedesktop/login1"
#define LOGIND_USER_PATH LOGIND_PATH "/user/self"

#define SYSTEMD_NAME "org.freedesktop.systemd1"
#define SYSTEMD_MANAGER_IFACE SYSTEMD_NAME ".Manager"
#define SYSTEMD_PATH "/org/freedesktop/systemd1"

static void
logind_on_session_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, UNUSED GVariant *params,
                         gpointer user_data)
{
    DBusContext *dc = (DBusContext *)user_data;

    if (!dc->logind_lock_func)
        return;

    if (g_strcmp0(signal, "Lock") == 0)
        dc->logind_lock_func(TRUE);
    else if (g_strcmp0(signal, "Unlock") == 0)
        dc->logind_lock_func(FALSE);
}

static void
logind_on_manager_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, GVariant *params, gpointer user_data)
{
    DBusContext *dc = (DBusContext *)user_data;
    gboolean state;

    if (g_strcmp0(signal, "PrepareForSleep") == 0) {
        if (!dc->logind_sleep_func)
            return;
        g_variant_get(params, "(b)", &state);
        dc->logind_sleep_func(state);
    } else if (g_strcmp0(signal, "PrepareForShutdown") == 0) {
        if (!dc->logind_shutdown_func)
            return;
        g_variant_get(params, "(b)", &state);
        dc->logind_shutdown_func(state);
    }
}

static gchar *
logind_get_session_path(GDBusConnection *conn)
{
    GError *err = NULL;
    GDBusProxy *proxy = g_dbus_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, NULL, LOGIND_NAME,
        LOGIND_USER_PATH, LOGIND_USER_IFACE, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return NULL;
    }

    GVariant *display = g_dbus_proxy_get_cached_property(proxy, "Display");

    if (!display) {
        g_warning("%s does not have Display property", LOGIND_USER_IFACE);
        g_object_unref(proxy);
        return NULL;
    }

    gchar *path = NULL;
    g_variant_get_child(display, 1, "o", &path);

    if (!path)
        g_warning("Failed to read Display session object path");

    g_variant_unref(display);
    g_object_unref(proxy);

    return path;
}

static void
logind_on_appear(GDBusConnection *conn, const gchar *name, const gchar *owner,
                 gpointer user_data)
{
    g_debug("%s appeared (owned by %s)", name, owner);

    DBusContext *dc = (DBusContext *)user_data;
    gchar *path = logind_get_session_path(conn);
    GError *err = NULL;

    if (path) {
        dc->logind_session = g_dbus_proxy_new_sync(
            conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, LOGIND_NAME,
            path, LOGIND_SESSION_IFACE, NULL, &err);

        g_free(path);

        if (err) {
            g_warning("%s", err->message);
            g_error_free(err);
            err = NULL;
        } else {
            g_signal_connect(dc->logind_session, "g-signal",
                             G_CALLBACK(logind_on_session_signal), user_data);
        }
    }

    dc->logind_manager = g_dbus_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, LOGIND_NAME,
        LOGIND_PATH, LOGIND_MANAGER_IFACE, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_signal_connect(dc->logind_manager, "g-signal",
                         G_CALLBACK(logind_on_manager_signal), user_data);
    }
}

static void
logind_free(DBusContext *dc)
{
    if (dc->logind_watcher) {
        g_bus_unwatch_name(dc->logind_watcher);
        dc->logind_watcher = 0;
    }

    if (dc->logind_session) {
        g_object_unref(dc->logind_session);
        dc->logind_session = NULL;
    }

    if (dc->logind_manager) {
        g_object_unref(dc->logind_manager);
        dc->logind_manager = NULL;
    }
}

static void
logind_on_vanish(UNUSED GDBusConnection *conn, const gchar *name,
                 gpointer user_data)
{
    g_debug("%s vanished", name);
    logind_free((DBusContext *)user_data);
}

static void
systemd_on_appear(GDBusConnection *conn, const gchar *name, const gchar *owner,
                  gpointer user_data)
{
    g_debug("%s appeared (owned by %s)", name, owner);

    DBusContext *dc = (DBusContext *)user_data;
    GError *err = NULL;

    dc->systemd_manager = g_dbus_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
        G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, NULL, SYSTEMD_NAME,
        SYSTEMD_PATH, SYSTEMD_MANAGER_IFACE, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    }
}

static void
systemd_free(DBusContext *dc)
{
    if (dc->systemd_watcher) {
        g_bus_unwatch_name(dc->systemd_watcher);
        dc->systemd_watcher = 0;
    }

    if (dc->systemd_manager) {
        g_object_unref(dc->systemd_manager);
        dc->systemd_manager = NULL;
    }
}

static void
systemd_on_vanish(UNUSED GDBusConnection *conn, const gchar *name,
                  gpointer user_data)
{
    g_debug("%s vanished", name);
    systemd_free((DBusContext *)user_data);
}

void
systemd_start_unit(DBusContext *dc, const gchar *name)
{
    if (!dc->systemd_manager) {
        g_warning("Cannot start unit %s: %s does not exist", name,
                  SYSTEMD_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(dc->systemd_manager, "StartUnit",
                           g_variant_new("(ss)", name, "replace"),
                           G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("Started unit %s", name);
    }
}

void
logind_set_idle(DBusContext *dc, gboolean state)
{
    if (!dc->logind_session) {
        g_warning("Cannot set IdleHint: %s does not exist", LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(dc->logind_session, "SetIdleHint",
                           g_variant_new("(b)", state), G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("IdleHint set to %s", BOOLSTR(state));
    }
}

void
logind_lock_session(DBusContext *dc, gboolean state)
{
#define STR(b) ((b) ? "Lock" : "Unlock")

    if (!dc->logind_session) {
        g_warning("Cannot %s session: %s does not exist", STR(state),
                  LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(dc->logind_session, STR(state), NULL,
                           G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("%sed session", STR(state));
    }

#undef STR
}

void
logind_set_locked_hint(DBusContext *dc, gboolean state)
{
    if (!dc->logind_session) {
        g_warning("Cannot set LockedHint: %s does not exist", LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(dc->logind_session, "SetLockedHint",
                           g_variant_new("(b)", state), G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("LockedHint set to %s", BOOLSTR(state));
    }
}

DBusContext *
dbus_new(void)
{
    DBusContext *dc = g_malloc0(sizeof(DBusContext));

    dc->logind_watcher = g_bus_watch_name(
        G_BUS_TYPE_SYSTEM, LOGIND_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        logind_on_appear, logind_on_vanish, (gpointer)dc, NULL);

    dc->systemd_watcher = g_bus_watch_name(
        G_BUS_TYPE_SESSION, SYSTEMD_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        systemd_on_appear, systemd_on_vanish, (gpointer)dc, NULL);

    return dc;
}

void
dbus_free(DBusContext *dc)
{
    logind_free(dc);
    systemd_free(dc);
    g_free(dc);
}
