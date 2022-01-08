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

#pragma once

#include <glib-2.0/glib.h>
#include <wp/wp.h>

typedef enum {
    AS_ACTION_ADD,
    AS_ACTION_REMOVE,
    AS_ACTION_CHANGE,
    AS_ACTION_CHANGE_DEFAULT,
} AudioSinkAction;

struct AudioSink {
    guint32 id;
    const gchar *name;
    gboolean mute;
    gdouble volume;
};

typedef void (*AudioSinkFunc)(AudioSinkAction a, guint32 id,
        struct AudioSink *as);

typedef struct {
    WpCore *core;
    WpObjectManager *manager;
    guint pending_plugins;
    WpPlugin *mixer_api;
    WpPlugin *nodes_api;
    guint32 default_id;
    AudioSinkFunc func;
} WpConn;

extern gboolean
audiosink_set_volume(guint32 id, gdouble v, WpConn *conn);
extern gboolean
audiosink_set_mute(guint32 id, gboolean m, WpConn *conn);
extern gboolean
audiosink_get_volume_mute(WpConn *conn, guint32 id, gdouble *v, gboolean *m);
extern WpConn *
wireplumber_connect(GMainContext *ctx, AudioSinkFunc func);
extern void
wireplumber_disconnect(WpConn *conn);
