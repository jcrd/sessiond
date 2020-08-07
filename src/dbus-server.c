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
#include "backlight.h"
#include "common.h"
#include "version.h"

#include <stdio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define DBUS_NAME "org.sessiond.session1"
#define DBUS_SESSION_ERROR DBUS_NAME ".Session.Error"
#define DBUS_BACKLIGHT_ERROR DBUS_NAME ".Backlight.Error"
#define DBUS_PATH "/org/sessiond/session1"
#define DBUS_BACKLIGHT_PATH DBUS_PATH "/backlight"

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

    GVariant *inhibit = g_variant_new("(ss)", who, why);
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

    gchar *who;
    gchar *why;
    GVariant *v = g_hash_table_lookup(s->inhibitors, id);
    if (!v) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_SESSION_ERROR ".Uninhibit", "Inhibitor ID does not exist");
        return TRUE;
    }
    g_variant_get(v, "(ss)", &who, &why);

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
    gchar *who;
    gchar *why;

    g_hash_table_iter_init(&iter, s->inhibitors);

    while (g_hash_table_iter_next(&iter, &k, &v)) {
        g_variant_get(v, "(ss)", &who, &why);
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

    g_variant_builder_init(&b, G_VARIANT_TYPE("a{s(ss)}"));

    GHashTableIter iter;
    gpointer k;
    gpointer v;

    g_hash_table_iter_init(&iter, s->inhibitors);

    while (g_hash_table_iter_next(&iter, &k, &v))
        g_variant_builder_add(&b, "{s@(ss)}", k, v);

    dict = g_variant_builder_end(&b);
    dbus_session_complete_list_inhibitors(session, i, dict);

    g_variant_ref_sink(dict);
    g_variant_unref(dict);

    return TRUE;
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
export_backlight(DBusServer *s, DBusBacklight *dbl)
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

static void
unexport_backlight(DBusServer *s, DBusBacklight *dbl)
{
    GDBusInterfaceSkeleton *skel = G_DBUS_INTERFACE_SKELETON(dbl);
    gchar *path = g_strdup(g_dbus_interface_skeleton_get_object_path(skel));

    g_dbus_interface_skeleton_unexport(skel);
    set_backlights_property(s);

    dbus_session_emit_remove_backlight(s->session, path);
    g_free(path);
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
            export_backlight(s, i->data);
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
        unexport_backlight(s, i->data);
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
        export_backlight(s, dbl);
}

void
dbus_server_remove_backlight(DBusServer *s, const char *path)
{
    DBusBacklight *dbl = g_hash_table_lookup(s->backlights, path);

    if (!dbl)
        return;

    g_hash_table_steal(s->backlights, path);
    unexport_backlight(s, dbl);
    g_object_unref(dbl);
}

void
dbus_server_update_backlight(DBusServer *s, struct Backlight *bl)
{
    DBusBacklight *dbl = g_hash_table_lookup(s->backlights, bl->sys_path);

    if (dbl)
        update_backlight(dbl, bl);
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
