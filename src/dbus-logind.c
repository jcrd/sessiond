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

#include "dbus-logind.h"
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

static void
logind_on_session_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, UNUSED GVariant *params,
                         gpointer user_data)
{
    LogindContext *c = (LogindContext *)user_data;

    if (!c->logind_lock_func)
        return;

    if (g_strcmp0(signal, "Lock") == 0)
        c->logind_lock_func(TRUE);
    else if (g_strcmp0(signal, "Unlock") == 0)
        c->logind_lock_func(FALSE);
}

static void
logind_on_manager_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, GVariant *params, gpointer user_data)
{
    LogindContext *c = (LogindContext *)user_data;
    gboolean state;

    if (g_strcmp0(signal, "PrepareForSleep") == 0) {
        if (!c->logind_sleep_func)
            return;
        g_variant_get(params, "(b)", &state);
        c->logind_sleep_func(state);
    } else if (g_strcmp0(signal, "PrepareForShutdown") == 0) {
        if (!c->logind_shutdown_func)
            return;
        g_variant_get(params, "(b)", &state);
        c->logind_shutdown_func(state);
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

    LogindContext *c = (LogindContext *)user_data;
    gchar *path = logind_get_session_path(conn);
    GError *err = NULL;

    if (path) {
        c->logind_session = g_dbus_proxy_new_sync(
            conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, LOGIND_NAME,
            path, LOGIND_SESSION_IFACE, NULL, &err);

        g_free(path);

        if (err) {
            g_warning("%s", err->message);
            g_error_free(err);
            err = NULL;
        } else {
            g_signal_connect(c->logind_session, "g-signal",
                             G_CALLBACK(logind_on_session_signal), user_data);
        }
    }

    c->logind_manager = g_dbus_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, LOGIND_NAME,
        LOGIND_PATH, LOGIND_MANAGER_IFACE, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_signal_connect(c->logind_manager, "g-signal",
                         G_CALLBACK(logind_on_manager_signal), user_data);
    }
}

static void
logind_on_vanish(UNUSED GDBusConnection *conn, const gchar *name,
                 gpointer user_data)
{
    g_debug("%s vanished", name);
    logind_free((LogindContext *)user_data);
}

void
logind_set_idle(LogindContext *c, gboolean state)
{
    if (!c->logind_session) {
        g_warning("Cannot set IdleHint: %s does not exist", LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(c->logind_session, "SetIdleHint",
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
logind_lock_session(LogindContext *c, gboolean state)
{
#define STR(b) ((b) ? "Lock" : "Unlock")

    if (!c->logind_session) {
        g_warning("Cannot %s session: %s does not exist", STR(state),
                  LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(c->logind_session, STR(state), NULL,
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
logind_set_locked_hint(LogindContext *c, gboolean state)
{
    if (!c->logind_session) {
        g_warning("Cannot set LockedHint: %s does not exist", LOGIND_NAME);
        return;
    }

    GError *err = NULL;
    g_dbus_proxy_call_sync(c->logind_session, "SetLockedHint",
                           g_variant_new("(b)", state), G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
    } else {
        g_debug("LockedHint set to %s", BOOLSTR(state));
    }
}

LogindContext *
logind_new(void)
{
    LogindContext *c = g_malloc0(sizeof(LogindContext));

    c->logind_watcher = g_bus_watch_name(
        G_BUS_TYPE_SYSTEM, LOGIND_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        logind_on_appear, logind_on_vanish, c, NULL);

    return c;
}

void
logind_free(LogindContext *c)
{
    if (!c)
        return;
    if (c->logind_watcher) {
        g_bus_unwatch_name(c->logind_watcher);
        c->logind_watcher = 0;
    }
    if (c->logind_session) {
        g_object_unref(c->logind_session);
        c->logind_session = NULL;
    }
    if (c->logind_manager) {
        g_object_unref(c->logind_manager);
        c->logind_manager = NULL;
    }
    g_free(c);
}
