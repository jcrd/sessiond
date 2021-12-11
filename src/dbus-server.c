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

#include "dbus-server.h"
#include "dbus-logind.h"
#include "dbus-gen.h"
#include "dbus-backlight.h"
#include "common.h"
#include "version.h"

#include <stdio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define EXPORTED(i) (g_dbus_interface_skeleton_get_object_path(\
            G_DBUS_INTERFACE_SKELETON(i)) != NULL)

G_DEFINE_TYPE(DBusServer, dbus_server, G_TYPE_OBJECT);

enum {
    INHIBIT_SIGNAL,
    UNINHIBIT_SIGNAL,
    LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static gboolean
on_handle_lock(DBusSession *session, GDBusMethodInvocation *i,
        gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;

    if (logind_get_locked_hint(s->ctx)) {
        gchar *msg = g_strdup_printf("Session %s is already locked",
                s->ctx->session_id);
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_SESSION_ERROR ".Lock", msg);
        g_free(msg);
        return TRUE;
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

    if (!logind_get_locked_hint(s->ctx)) {
        gchar *msg = g_strdup_printf("Session %s is not locked",
                s->ctx->session_id);
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_SESSION_ERROR ".Unlock", msg);
        g_free(msg);
        return TRUE;
    }

    logind_lock_session(s->ctx, FALSE);
    dbus_session_complete_unlock(session, i);

    return TRUE;
}

static gboolean
on_handle_inhibit(DBusSession *session, GDBusMethodInvocation *i,
        const gchar *who, const gchar *why, gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    gchar *id = g_uuid_string_random();

    GVariant *inhibit = g_variant_new("(tss)", g_get_real_time(), who, why);
    g_variant_ref_sink(inhibit);
    g_hash_table_insert(s->inhibitors, id, inhibit);

    g_signal_emit(s, signals[INHIBIT_SIGNAL], 0, who, why,
            g_hash_table_size(s->inhibitors));

    dbus_session_complete_inhibit(session, i, id);

    return TRUE;
}

static gboolean
on_handle_uninhibit(DBusSession *session, GDBusMethodInvocation *i,
        const gchar *id, gpointer user_data)
{
    if (!g_uuid_string_is_valid(id)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_SESSION_ERROR ".Uninhibit", "Inhibitor ID is not valid");
        return TRUE;
    }

    DBusServer *s = (DBusServer *)user_data;

    gint64 timestamp;
    gchar *who;
    gchar *why;
    GVariant *v = g_hash_table_lookup(s->inhibitors, id);
    if (!v) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_SESSION_ERROR ".Uninhibit", "Inhibitor ID does not exist");
        return TRUE;
    }
    g_variant_get(v, "(tss)", &timestamp, &who, &why);

    g_hash_table_remove(s->inhibitors, id);
    guint n = g_hash_table_size(s->inhibitors);

    g_signal_emit(s, signals[UNINHIBIT_SIGNAL], 0, who, why, n);

    g_free(who);
    g_free(why);

    dbus_session_complete_uninhibit(session, i);

    return TRUE;
}

static gboolean
on_handle_stop_inhibitors(DBusSession *session, GDBusMethodInvocation *i,
        gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    guint count = g_hash_table_size(s->inhibitors);
    guint n = count;

    GHashTableIter iter;
    gpointer k;
    gpointer v;
    gint64 timestamp;
    gchar *who;
    gchar *why;

    g_hash_table_iter_init(&iter, s->inhibitors);

    while (g_hash_table_iter_next(&iter, &k, &v)) {
        g_variant_get(v, "(tss)", &timestamp, &who, &why);
        g_hash_table_iter_remove(&iter);

        g_signal_emit(s, signals[UNINHIBIT_SIGNAL], 0, who, why, --n);

        g_free(who);
        g_free(why);
    }

    dbus_session_complete_stop_inhibitors(session, i, count);

    return TRUE;
}

static gboolean
on_handle_list_inhibitors(DBusSession *session, GDBusMethodInvocation *i,
        gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    GVariantBuilder b;
    GVariant *dict;

    g_variant_builder_init(&b, G_VARIANT_TYPE("a{s(tss)}"));

    GHashTableIter iter;
    gpointer k;
    gpointer v;

    g_hash_table_iter_init(&iter, s->inhibitors);

    while (g_hash_table_iter_next(&iter, &k, &v))
        g_variant_builder_add(&b, "{s@(tss)}", k, v);

    dict = g_variant_builder_end(&b);
    dbus_session_complete_list_inhibitors(session, i, dict);

    g_variant_ref_sink(dict);
    g_variant_unref(dict);

    return TRUE;
}

static void
lock_callback(LogindContext *c, gboolean state, gpointer data)
{
    DBusServer *s = (DBusServer *)data;
    if (!EXPORTED(s->session))
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
    if (!EXPORTED(s->session))
        return;
    dbus_session_emit_prepare_for_sleep(s->session, state);
}

static void
shutdown_callback(UNUSED LogindContext *c, gboolean state, gpointer data)
{
    DBusServer *s = (DBusServer *)data;
    if (!EXPORTED(s->session))
        return;
    dbus_session_emit_prepare_for_shutdown(s->session, state);
}

static void
on_properties_changed(UNUSED GDBusProxy *proxy, GVariant *props,
        UNUSED GStrv inv_props, gpointer user_data)
{
    DBusServer *s = (DBusServer *)user_data;
    if (!EXPORTED(s->session))
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

    dbus_session_set_inhibited_hint(s->session,
            g_hash_table_size(s->inhibitors) > 0);
    dbus_session_set_locked_hint(s->session, logind_get_locked_hint(c));
    dbus_session_set_idle_hint(s->session, logind_get_idle_hint(c));
    dbus_session_set_idle_since_hint(s->session, logind_get_idle_since_hint(c));
    dbus_session_set_idle_since_hint_monotonic(s->session,
            logind_get_idle_since_hint_monotonic(c));

    dbus_session_set_version(s->session, VERSION);
}

static void
on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    g_debug("%s acquired", name);

    DBusServer *s = (DBusServer *)user_data;
    s->conn = conn;
    s->name_acquired = TRUE;

    DBusSession *session = dbus_session_skeleton_new();

    if (!session) {
        g_error("Failed to initialize DBus Session");
        return;
    }

    s->session = session;
    LogindContext *c = s->ctx;

    g_signal_connect(session, "handle-lock", G_CALLBACK(on_handle_lock), s);
    g_signal_connect(session, "handle-unlock", G_CALLBACK(on_handle_unlock), s);
    g_signal_connect(session, "handle-inhibit",
            G_CALLBACK(on_handle_inhibit), s);
    g_signal_connect(session, "handle-uninhibit",
            G_CALLBACK(on_handle_uninhibit), s);
    g_signal_connect(session, "handle-stop-inhibitors",
            G_CALLBACK(on_handle_stop_inhibitors), s);
    g_signal_connect(session, "handle-list-inhibitors",
            G_CALLBACK(on_handle_list_inhibitors), s);

    g_signal_connect_after(c, "lock", G_CALLBACK(lock_callback), s);
    g_signal_connect_after(c, "sleep", G_CALLBACK(sleep_callback), s);
    g_signal_connect_after(c, "shutdown", G_CALLBACK(shutdown_callback), s);

    g_signal_connect(c->logind_session, "g-properties-changed",
            G_CALLBACK(on_properties_changed), s);
    init_properties(s);

    GError *err = NULL;
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(session),
            conn, DBUS_PATH, &err);
    if (err) {
        g_error("Failed to export DBus Session interface: %s", err->message);
        g_error_free(err);
        err = NULL;
    }

    GList *dbls = g_hash_table_get_values(s->backlights);
    for (GList *i = dbls; i; i = i->next)
        if (!EXPORTED(i->data))
            dbus_server_export_backlight(s, i->data);
    g_list_free(dbls);
}

static void
on_name_lost(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    g_debug("%s lost", name);

    DBusServer *s = (DBusServer *)user_data;
    s->name_acquired = FALSE;

    g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(s->session));

    GList *dbls = g_hash_table_get_values(s->backlights);
    for (GList *i = dbls; i; i = i->next)
        dbus_server_unexport_backlight(s, i->data);
    g_list_free(dbls);

    s->bus_id = 0;
}

void
dbus_server_free(DBusServer *s)
{
    if (!s)
        return;
    if (s->bus_id)
        g_bus_unown_name(s->bus_id);
    if (s->session)
        g_object_unref(s->session);
    g_hash_table_destroy(s->backlights);
    g_hash_table_destroy(s->inhibitors);
    g_object_unref(s);
}

static void
dbus_server_class_init(DBusServerClass *s)
{
    GType type = G_OBJECT_CLASS_TYPE(s);

    signals[INHIBIT_SIGNAL] = g_signal_new("inhibit",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
    signals[UNINHIBIT_SIGNAL] = g_signal_new("uninhibit",
            type, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
}

static void
dbus_server_init(DBusServer *self)
{
}

void
dbus_server_emit_active(DBusServer *s)
{
    if (!s || !EXPORTED(s->session))
        return;
    dbus_session_emit_active(s->session);
}

void
dbus_server_emit_inactive(DBusServer *s, guint i)
{
    if (!s || !EXPORTED(s->session))
        return;
    dbus_session_emit_inactive(s->session, i);
}

DBusServer *
dbus_server_new(LogindContext *c)
{
    DBusServer *s = g_object_new(DBUS_TYPE_SERVER, NULL);

    s->ctx = c;
    s->bl_devices = NULL;
    s->backlights = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
            g_object_unref);
    s->inhibitors = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
            (GDestroyNotify)g_variant_unref);

    s->bus_id = g_bus_own_name(G_BUS_TYPE_SESSION, DBUS_NAME,
            G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
            G_BUS_NAME_OWNER_FLAGS_REPLACE,
            NULL, on_name_acquired, on_name_lost, s, NULL);

    return s;
}
