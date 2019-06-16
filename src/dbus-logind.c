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

G_DEFINE_TYPE(LogindContext, logind_context, G_TYPE_OBJECT);

enum {
    LOCK_SIGNAL,
    SLEEP_SIGNAL,
    SHUTDOWN_SIGNAL,
    APPEAR_SIGNAL,
    VANISH_SIGNAL,
    LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void
logind_on_session_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, UNUSED GVariant *params,
                         gpointer user_data)
{
    LogindContext *c = (LogindContext *)user_data;

    if (g_strcmp0(signal, "Lock") == 0) {
        g_debug("Lock signal received");
        g_signal_emit(c, signals[LOCK_SIGNAL], 0, TRUE);
    } else if (g_strcmp0(signal, "Unlock") == 0) {
        g_debug("Unlock signal received");
        g_signal_emit(c, signals[LOCK_SIGNAL], 0, FALSE);
    }
}

static void
logind_on_manager_signal(UNUSED GDBusProxy *proxy, UNUSED gchar *sender,
                         gchar *signal, GVariant *params, gpointer user_data)
{
    LogindContext *c = (LogindContext *)user_data;

    gboolean state;
    g_variant_get(params, "(b)", &state);

    if (g_strcmp0(signal, "PrepareForSleep") == 0) {
        g_debug("PrepareForSleep signal received: %s", BOOLSTR(state));
        g_signal_emit(c, signals[SLEEP_SIGNAL], 0, state);
    } else if (g_strcmp0(signal, "PrepareForShutdown") == 0) {
        g_debug("PrepareForShutdown signal received: %s", BOOLSTR(state));
        g_signal_emit(c, signals[SHUTDOWN_SIGNAL], 0, state);
    }
}

static gchar *
logind_get_session(GDBusConnection *conn, gchar **id)
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

    gchar *path = NULL;
    GVariant *display = g_dbus_proxy_get_cached_property(proxy, "Display");

    if (!display) {
        g_warning("%s does not have Display property", LOGIND_USER_IFACE);
        goto error;
    }

    g_variant_get(display, "(so)", id, &path);

    if (!*id) {
        g_warning("Failed to read Display session Id");
        goto error;
    }

    if (!path)
        g_warning("Failed to read Display session object path");

error:
    if (display)
        g_variant_unref(display);
    g_object_unref(proxy);

    return path;
}

static void
logind_get_property(LogindContext *c, const gchar *prop, const gchar *fmt,
        gpointer out)
{
    GVariant *v = g_dbus_proxy_get_cached_property(c->logind_session,
            prop);
    if (v)
        g_variant_get(v, fmt, out);
    else
        g_warning("Failed to get logind %s", prop);
    g_variant_unref(v);
}

gboolean
logind_get_locked_hint(LogindContext *c)
{
    gboolean b = FALSE;
    logind_get_property(c, "LockedHint", "b", &b);
    return b;
}

gboolean
logind_get_idle_hint(LogindContext *c)
{
    gboolean b = FALSE;
    logind_get_property(c, "IdleHint", "b", &b);
    return b;
}

guint64
logind_get_idle_since_hint(LogindContext *c)
{
    guint64 i = 0;
    logind_get_property(c, "IdleSinceHint", "t", &i);
    return i;
}

guint64
logind_get_idle_since_hint_monotonic(LogindContext *c)
{
    guint64 i = 0;
    logind_get_property(c, "IdleSinceHintMonotonic", "t", &i);
    return i;
}

static void
logind_on_appear(GDBusConnection *conn, const gchar *name, const gchar *owner,
                 gpointer user_data)
{
    g_debug("%s appeared (owned by %s)", name, owner);

    LogindContext *c = (LogindContext *)user_data;
    gchar *path = logind_get_session(conn, &c->session_id);
    GError *err = NULL;

    if (path) {
        c->logind_session = g_dbus_proxy_new_sync(
            conn, G_DBUS_PROXY_FLAGS_NONE, NULL, LOGIND_NAME,
            path, LOGIND_SESSION_IFACE, NULL, &err);

        if (err) {
            g_warning("%s", err->message);
            g_error_free(err);
            err = NULL;
        } else {
            g_signal_connect(c->logind_session, "g-signal",
                             G_CALLBACK(logind_on_session_signal), user_data);
            g_debug("Using logind session %s: %s", c->session_id, path);
        }
        g_free(path);
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

    g_signal_emit(c, signals[APPEAR_SIGNAL], 0);
}

static void
logind_on_vanish(UNUSED GDBusConnection *conn, const gchar *name,
                 gpointer user_data)
{
    g_debug("%s vanished", name);

    LogindContext *c = (LogindContext *)user_data;
    g_signal_emit(c, signals[VANISH_SIGNAL], 0);
    logind_context_free(c);
}

static void
logind_context_class_init(LogindContextClass *c)
{
    GType type = G_OBJECT_CLASS_TYPE(c);

    signals[LOCK_SIGNAL] = g_signal_new("lock",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);
    signals[SLEEP_SIGNAL] = g_signal_new("sleep",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);
    signals[SHUTDOWN_SIGNAL] = g_signal_new("shutdown",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
            G_TYPE_BOOLEAN);
    signals[APPEAR_SIGNAL] = g_signal_new("appear",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    signals[VANISH_SIGNAL] = g_signal_new("vanish",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
logind_context_init(LogindContext *self)
{
    self->logind_watcher = g_bus_watch_name(
        G_BUS_TYPE_SYSTEM, LOGIND_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        logind_on_appear, logind_on_vanish, self, NULL);
}

void
logind_set_idle_hint(LogindContext *c, gboolean state)
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

/* Locking session automatically updates logind LockedHint. */
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

/* Set LockedHint manually when responding to Lock signal. */
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
logind_context_new(void)
{
    return g_object_new(LOGIND_TYPE_CONTEXT, NULL);
}

void
logind_context_free(LogindContext *c)
{
    if (!c)
        return;
    if (c->session_id)
        g_free(c->session_id);
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
    g_object_unref(c);
}
