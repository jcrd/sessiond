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

#define G_LOG_DOMAIN "sessiond-wireplumber"

#include "wireplumber.h"

#include <glib-2.0/glib.h>
#include <pipewire/keys.h>
#include <wp/proxy.h>
#include <wp/wp.h>

#define WP_MIXER_API "libwireplumber-module-mixer-api"
#define WP_DEFAULT_NODES_API "libwireplumber-module-default-nodes-api"

static const gchar *
get_object_name(WpPipewireObject *obj)
{
    const gchar *name =
        wp_pipewire_object_get_property(obj, PW_KEY_NODE_DESCRIPTION);
    if (!name)
        name = wp_pipewire_object_get_property(obj, PW_KEY_APP_NAME);
    if (!name)
        name = wp_pipewire_object_get_property(obj, PW_KEY_NODE_NAME);

    return name;
}

static void
update_default_id(WpConn *conn)
{
    guint32 id;
    g_signal_emit_by_name(conn->nodes_api, "get-default-node", "Audio/Sink",
            &id);

    if (id == conn->default_id)
        return;

    conn->default_id = id;
    conn->func(AS_ACTION_CHANGE_DEFAULT, id, NULL);
}

static void
on_disconnect(WpConn *conn)
{
    if (!conn)
        return;
    if (conn->mixer_api)
        g_object_unref(conn->mixer_api);
    if (conn->nodes_api)
        g_object_unref(conn->nodes_api);
}

static struct AudioSink *
new_audiosink(WpConn *conn, guint32 id, WpPipewireObject *obj)
{
    GVariant *dict = NULL;
    g_signal_emit_by_name(conn->mixer_api, "get-volume", id, &dict);
    if (!dict) {
        g_warning("Failed to get audio sink volume: %d", id);
        return NULL;
    }

    struct AudioSink *as = g_malloc0(sizeof(struct AudioSink));

    as->id = id;
    if (obj)
        as->name = get_object_name(obj);

    g_variant_lookup(dict, "mute", "b", &as->mute);
    g_variant_lookup(dict, "volume", "d", &as->volume);
    g_variant_unref(dict);

    return as;
}

static void
on_object_changed(WpConn *conn, guint32 id)
{
    struct AudioSink *as = new_audiosink(conn, id, NULL);
    conn->func(AS_ACTION_CHANGE, id, as);
    g_free(as);
}

static void
on_object_added(WpConn *conn, WpPipewireObject *obj)
{
    guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));

    g_debug("Added audio sink: %d", id);

    g_signal_connect_swapped(conn->mixer_api, "changed",
            G_CALLBACK(on_object_changed), conn);

    struct AudioSink *as = new_audiosink(conn, id, obj);
    conn->func(AS_ACTION_ADD, id, as);
    g_free(as);
}

static void
on_object_removed(WpConn *conn, WpPipewireObject *obj)
{
    guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));

    g_debug("Removed audio sink: %d", id);

    conn->func(AS_ACTION_REMOVE, id, NULL);
}

static void
on_plugin_activated(GObject *obj, GAsyncResult *res, gpointer data)
{
    WpObject *p = (WpObject *)obj;
    WpConn *conn = (WpConn *)data;
    GError *err = NULL;

    if (!wp_object_activate_finish(p, res, &err)) {
        g_warning("%s", err->message);
        return;
    }

    if (--conn->pending_plugins == 0)
        wp_core_install_object_manager(conn->core, conn->manager);
}

gboolean
audiosink_set_volume(guint32 id, gdouble v, WpConn *conn)
{
    GVariantBuilder b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&b, "{sv}", "volume", g_variant_new_double(v));
    GVariant *variant = g_variant_builder_end(&b);

    gboolean res = FALSE;
    g_signal_emit_by_name(conn->mixer_api, "set-volume", id, variant,
            &res);

    if (!res) {
        g_warning("Failed to set audio sink volume: %d", id);
        return FALSE;
    }

    return TRUE;
}

gboolean
audiosink_set_mute(guint32 id, gboolean m, WpConn *conn)
{
    GVariantBuilder b = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&b, "{sv}", "mute", g_variant_new_boolean(m));
    GVariant *variant = g_variant_builder_end(&b);

    gboolean res = FALSE;
    g_signal_emit_by_name(conn->mixer_api, "set-volume", id, variant,
            &res);

    if (!res) {
        g_warning("Failed to set audio sink mute state: %d", id);
        return FALSE;
    }

    return TRUE;
}

WpConn *
wireplumber_connect(GMainContext *ctx, AudioSinkFunc func)
{
    wp_init(WP_INIT_ALL & ~WP_INIT_SET_GLIB_LOG);

    WpConn *conn = g_malloc0(sizeof(WpConn));
    conn->core = wp_core_new(ctx, NULL);
    conn->manager = wp_object_manager_new();
    conn->func = func;

    wp_object_manager_add_interest(conn->manager, WP_TYPE_NODE,
        WP_CONSTRAINT_TYPE_PW_PROPERTY, PW_KEY_MEDIA_CLASS, "#s", "*/Sink*",
        NULL);

    GError *err = NULL;

    /* Load API modules. */
    if (!wp_core_load_component(conn->core, WP_MIXER_API, "module",
                NULL, &err)) {
        g_warning("%s", err->message);
        goto error;
    }

    if (!wp_core_load_component(conn->core, WP_DEFAULT_NODES_API, "module",
                NULL, &err)) {
        g_warning("%s", err->message);
        goto error;
    }

    if (!wp_core_connect(conn->core)) {
        g_warning("Failed to connect to WirePlumber");
        goto error;
    }

    /* Activate API modules. */
    conn->mixer_api = wp_plugin_find(conn->core, "mixer-api");
    g_object_set(G_OBJECT(conn->mixer_api), "scale", 1 /* cubic */, NULL);

    conn->nodes_api = wp_plugin_find(conn->core, "default-nodes-api");

    conn->pending_plugins = 2;

    wp_object_activate(WP_OBJECT(conn->mixer_api), WP_PLUGIN_FEATURE_ENABLED,
            NULL, (GAsyncReadyCallback)on_plugin_activated, conn);
    wp_object_activate(WP_OBJECT(conn->nodes_api), WP_PLUGIN_FEATURE_ENABLED,
            NULL, (GAsyncReadyCallback)on_plugin_activated, conn);

    /* Connect signals. */
    g_signal_connect_swapped(conn->core, "disconnected",
            G_CALLBACK(on_disconnect), conn);

    g_signal_connect_swapped(conn->manager, "installed",
            G_CALLBACK(update_default_id), conn);
    g_signal_connect_swapped(conn->manager, "object-added",
            G_CALLBACK(on_object_added), conn);
    g_signal_connect_swapped(conn->manager, "object-removed",
            G_CALLBACK(on_object_removed), conn);

    g_signal_connect_swapped(conn->nodes_api, "changed",
            G_CALLBACK(update_default_id), conn);

    return conn;

error:
    wireplumber_disconnect(conn);
    return NULL;
}

void
wireplumber_disconnect(WpConn *conn)
{
    if (conn && wp_core_is_connected(conn->core))
        wp_core_disconnect(conn->core);
}
