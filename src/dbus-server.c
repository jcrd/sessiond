#include "dbus-server.h"
#include "dbus-logind.h"
#include "dbus-gen.h"
#include "backlight.h"
#include "common.h"

#include <stdio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define DBUS_NAME "org.sessiond.session1"
#define DBUS_ERROR DBUS_NAME ".Error"
#define DBUS_PATH "/org/sessiond/session1"

G_DEFINE_TYPE(DBusServer, dbus_server, G_TYPE_OBJECT);

static gboolean
on_handle_lock(DBusSession *session, GDBusMethodInvocation *i,
        gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;

    if (logind_get_locked_hint(s->ctx)) {
        gchar *msg = g_strdup_printf("Session %s is already locked",
                s->ctx->session_id);
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_ERROR ".Lock", msg);
        g_free(msg);
        return FALSE;
    }

    logind_lock_session(s->ctx, TRUE);
    dbus_session_complete_lock(session, i);

    return TRUE;
}

static gboolean
on_handle_unlock(DBusSession *session, GDBusMethodInvocation *i,
        gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    logind_lock_session(s->ctx, FALSE);
    dbus_session_complete_unlock(session, i);
    return TRUE;
}

static void
lock_callback(LogindContext *c, gboolean state, gpointer data)
{
    DBusServer *s = (DBusServer *)data;
    if (!s->exported)
        return;
    if (state)
        dbus_session_emit_lock(s->session);
    else
        dbus_session_emit_unlock(s->session);
}

static void
sleep_callback(UNUSED LogindContext *c, gboolean state, gpointer data)
{
    DBusServer *s = (DBusServer *)data;
    if (!s->exported)
        return;
    dbus_session_emit_prepare_for_sleep(s->session, state);
}

static void
shutdown_callback(UNUSED LogindContext *c, gboolean state, gpointer data)
{
    DBusServer *s = (DBusServer *)data;
    if (!s->exported)
        return;
    dbus_session_emit_prepare_for_shutdown(s->session, state);
}

static void
on_properties_changed(UNUSED GDBusProxy *proxy, GVariant *props,
        UNUSED GStrv inv_props, gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    if (!s->exported)
        return;
    LogindContext *c = s->ctx;
    gchar *prop = NULL;
    GVariantIter iter;

    g_variant_iter_init(&iter, props);

    while (g_variant_iter_loop(&iter, "{sv}", &prop, NULL)) {
        if (g_strcmp0(prop, "LockedHint") == 0) {
            dbus_session_set_locked_hint(s->session,
                    logind_get_locked_hint(c));
        } else if (g_strcmp0(prop, "IdleHint") == 0) {
            gboolean idle = logind_get_idle_hint(c);
            dbus_session_set_idle_hint(s->session, idle);
            if (idle)
                dbus_session_emit_idle(s->session);
        } else if (g_strcmp0(prop, "IdleSinceHint") == 0) {
            dbus_session_set_idle_since_hint(s->session,
                    logind_get_idle_since_hint(c));
        } else if (g_strcmp0(prop, "IdleSinceHintMonotonic") == 0) {
            dbus_session_set_idle_since_hint_monotonic(s->session,
                    logind_get_idle_since_hint_monotonic(c));
        }
    }
}

static void
init_properties(DBusServer *s)
{
    LogindContext *c = s->ctx;

    dbus_session_set_locked_hint(s->session, logind_get_locked_hint(c));
    dbus_session_set_idle_hint(s->session, logind_get_idle_hint(c));
    dbus_session_set_idle_since_hint(s->session, logind_get_idle_since_hint(c));
    dbus_session_set_idle_since_hint_monotonic(s->session,
            logind_get_idle_since_hint_monotonic(c));
}

static void
on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    g_debug("%s acquired", name);

    DBusSession *session = dbus_session_skeleton_new();

    if (!session) {
        g_error("Failed to initialize DBus server");
        return;
    }

    DBusServer *s = (DBusServer *)user_data;
    s->session = session;
    LogindContext *c = s->ctx;

    g_signal_connect(session, "handle-lock", G_CALLBACK(on_handle_lock), s);
    g_signal_connect(session, "handle-unlock", G_CALLBACK(on_handle_unlock), s);

    g_signal_connect_after(c, "lock", G_CALLBACK(lock_callback), s);
    g_signal_connect_after(c, "sleep", G_CALLBACK(sleep_callback), s);
    g_signal_connect_after(c, "shutdown", G_CALLBACK(shutdown_callback), s);

    g_signal_connect(c->logind_session, "g-properties-changed",
            G_CALLBACK(on_properties_changed), s);
    init_properties(s);

    GError *err = NULL;
    s->exported = g_dbus_interface_skeleton_export(
            G_DBUS_INTERFACE_SKELETON(session),
            conn, DBUS_PATH, &err);
    if (err) {
        g_error("Failed to export DBus interface: %s", err->message);
        g_free(err);
    }
}

static void
on_name_lost(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    g_debug("%s lost", name);

    DBusServer *s = (DBusServer *)user_data;

    g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(s->session));
    s->exported = FALSE;
    s->bus_id = 0;
}

static void
dbus_server_destroy(DBusServer *s)
{
    g_object_unref(s->session);
    s->session = NULL;
}

void
dbus_server_free(DBusServer *s)
{
    if (s->bus_id)
        g_bus_unown_name(s->bus_id);
    if (s->session)
        g_object_unref(s->session);
    g_object_unref(s);
}

static void
dbus_server_class_init(DBusServerClass *s)
{
}

static void
dbus_server_init(DBusServer *self)
{
}

void
dbus_server_emit_active(DBusServer *s)
{
    if (!s->exported)
        return;
    dbus_session_emit_active(s->session);
}

void
dbus_server_emit_inactive(DBusServer *s, guint i)
{
    if (!s->exported)
        return;
    dbus_session_emit_inactive(s->session, i);
}

DBusServer *
dbus_server_new(LogindContext *c)
{
    DBusServer *s = g_object_new(DBUS_TYPE_SERVER, NULL);
    s->ctx = c;
    s->exported = FALSE;

    s->bus_id = g_bus_own_name(G_BUS_TYPE_SESSION, DBUS_NAME,
            G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
            G_BUS_NAME_OWNER_FLAGS_REPLACE,
            NULL, on_name_acquired, on_name_lost,
            s, (GDestroyNotify)dbus_server_destroy);

    return s;
}
