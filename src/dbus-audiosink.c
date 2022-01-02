/*
sessiond - standalone X session manager
Copyright (C) 2021 James Reed

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

#include "dbus-audiosink.h"
#include "dbus-server.h"
#include "dbus-gen.h"
#include "wireplumber.h"

#include <glib-2.0/glib.h>

#define INT_PTR(i) GINT_TO_POINTER(i)

static void
set_default_audio_sink_property(DBusServer *s, DBusAudioSink *das)
{
    const gchar *path = g_dbus_interface_skeleton_get_object_path(
            G_DBUS_INTERFACE_SKELETON(das));
    dbus_session_set_default_audio_sink(s->session, path);
    dbus_session_emit_change_default_audio_sink(s->session, path);
}

static void
set_audio_sinks_property(DBusServer *s)
{
    GList *dass = g_hash_table_get_values(s->audiosinks);
    const gchar **paths = g_malloc0_n(g_list_length(dass) + 1, sizeof(gchar *));

    guint n = 0;
    for (GList *i = dass; i; i = i->next, n++) {
        DBusAudioSink *das = i->data;
        const gchar *path = g_dbus_interface_skeleton_get_object_path(
                G_DBUS_INTERFACE_SKELETON(das));
        if (path)
            paths[n] = path;
    }

    dbus_session_set_audio_sinks(s->session, paths);
    g_free(paths);
}

static gboolean
on_handle_set_volume(DBusAudioSink *das, GDBusMethodInvocation *i,
        gdouble v, DBusServer *s)
{
    if (!s->wp_conn)
        return FALSE;

    guint32 id = dbus_audio_sink_get_id(das);

    if (!audiosink_set_volume(id, v, s->wp_conn)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_AUDIOSINK_ERROR ".SetVolume",
                "Failed to set volume");
        return TRUE;
    }

    dbus_audio_sink_complete_set_volume(das, i);

    return TRUE;
}

static gboolean
on_handle_inc_volume(DBusAudioSink *das, GDBusMethodInvocation *i,
        gdouble v, DBusServer *s)
{
    if (!s->wp_conn)
        return FALSE;

    guint32 id = dbus_audio_sink_get_id(das);
    gdouble vol = dbus_audio_sink_get_volume(das);
    vol = MIN(MAX(vol + v, 0.0), 1.0);

    if (!audiosink_set_volume(id, vol, s->wp_conn)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_AUDIOSINK_ERROR ".IncVolume",
                "Failed to increment volume");
        return TRUE;
    }

    dbus_audio_sink_complete_inc_volume(das, i, vol);

    return TRUE;
}

static gboolean
on_handle_set_mute(DBusAudioSink *das, GDBusMethodInvocation *i,
        gboolean m, DBusServer *s)
{
    if (!s->wp_conn)
        return FALSE;

    guint32 id = dbus_audio_sink_get_id(das);

    if (!audiosink_set_mute(id, m, s->wp_conn)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_AUDIOSINK_ERROR ".SetMute",
                "Failed to set mute state");
        return TRUE;
    }

    dbus_audio_sink_complete_set_mute(das, i);

    return TRUE;
}

static gboolean
on_handle_toggle_mute(DBusAudioSink *das, GDBusMethodInvocation *i,
        DBusServer *s)
{
    if (!s->wp_conn)
        return FALSE;

    guint32 id = dbus_audio_sink_get_id(das);
    gboolean mute = !dbus_audio_sink_get_mute(das);

    if (!audiosink_set_mute(id, mute, s->wp_conn)) {
        g_dbus_method_invocation_return_dbus_error(i,
                DBUS_AUDIOSINK_ERROR ".ToggleMute",
                "Failed to toggle mute state");
        return TRUE;
    }

    dbus_audio_sink_complete_toggle_mute(das, i, mute);

    return TRUE;
}

static void
update_audiosink(DBusAudioSink *das, struct AudioSink *as)
{
    if (as->name) {
        const gchar *str = dbus_audio_sink_get_name(das);
        if (!str || g_strcmp0(str, as->name) != 0)
            dbus_audio_sink_set_name(das, as->name);
    }

    if (dbus_audio_sink_get_id(das) != as->id)
        dbus_audio_sink_set_id(das, as->id);

    if (dbus_audio_sink_get_mute(das) != as->mute) {
        dbus_audio_sink_set_mute(das, as->mute);
        dbus_audio_sink_emit_change_mute(das, as->mute);
    }

    if (dbus_audio_sink_get_volume(das) != as->volume) {
        dbus_audio_sink_set_volume(das, as->volume);
        dbus_audio_sink_emit_change_volume(das, as->volume);
    }
}

gboolean
dbus_server_export_audiosink(DBusServer *s, DBusAudioSink *das)
{
    guint32 id = dbus_audio_sink_get_id(das);
    gchar *path = g_strdup_printf("%s/%d", DBUS_AUDIOSINK_PATH, id);

    GError *err = NULL;
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(das), s->conn,
            path, &err);

    if (err) {
        g_error("Failed to export DBus AudioSink interface: %s", err->message);
        g_error_free(err);
        g_free(path);
        return FALSE;
    }

    set_audio_sinks_property(s);

    dbus_session_emit_add_audio_sink(s->session, path);
    g_free(path);

    return TRUE;
}

void
dbus_server_unexport_audiosink(DBusServer *s, DBusAudioSink *das)
{
    GDBusInterfaceSkeleton *skel = G_DBUS_INTERFACE_SKELETON(das);
    gchar *path = g_strdup(g_dbus_interface_skeleton_get_object_path(skel));

    g_dbus_interface_skeleton_unexport(skel);
    set_audio_sinks_property(s);

    dbus_session_emit_remove_audio_sink(s->session, path);
    g_free(path);
}

void
dbus_server_add_audiosink(DBusServer *s, struct AudioSink *as)
{

    DBusAudioSink *das = dbus_audio_sink_skeleton_new();
    g_hash_table_insert(s->audiosinks, INT_PTR(as->id), das);

    g_signal_connect(das, "handle-set-volume",
            G_CALLBACK(on_handle_set_volume), s);
    g_signal_connect(das, "handle-inc-volume",
            G_CALLBACK(on_handle_inc_volume), s);
    g_signal_connect(das, "handle-set-mute",
            G_CALLBACK(on_handle_set_mute), s);
    g_signal_connect(das, "handle-toggle-mute",
            G_CALLBACK(on_handle_toggle_mute), s);

    update_audiosink(das, as);

    if (s->name_acquired)
        dbus_server_export_audiosink(s, das);
}

void
dbus_server_remove_audiosink(DBusServer *s, guint32 id)
{
    DBusAudioSink *das = g_hash_table_lookup(s->audiosinks, INT_PTR(id));

    if (!das)
        return;

    g_hash_table_steal(s->audiosinks, INT_PTR(id));
    dbus_server_unexport_audiosink(s, das);
    g_object_unref(das);
}

void
dbus_server_update_audiosink(DBusServer *s, struct AudioSink *as)
{
    DBusAudioSink *das = g_hash_table_lookup(s->audiosinks, INT_PTR(as->id));

    if (das)
        update_audiosink(das, as);
}

void
dbus_server_update_default_audiosink(DBusServer *s, guint32 id)
{
    DBusAudioSink *das = g_hash_table_lookup(s->audiosinks, INT_PTR(id));

    if (das)
        set_default_audio_sink_property(s, das);
}
