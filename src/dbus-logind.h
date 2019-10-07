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

#pragma once

#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>

#define LOGIND_TYPE_CONTEXT logind_context_get_type()
G_DECLARE_FINAL_TYPE(LogindContext, logind_context, LOGIND, CONTEXT, GObject);

struct _LogindContext {
    GObject parent;
    gchar *session_id;
    guint logind_watcher;
    GDBusProxy *logind_session;
    GDBusProxy *logind_manager;
};

extern void
logind_set_idle_hint(LogindContext *c, gboolean state);
extern void
logind_lock_session(LogindContext *c, gboolean state);
extern void
logind_set_locked_hint(LogindContext *c, gboolean state);
extern gboolean
logind_get_locked_hint(LogindContext *c);
extern gboolean
logind_get_idle_hint(LogindContext *c);
extern guint64
logind_get_idle_since_hint(LogindContext *c);
extern guint64
logind_get_idle_since_hint_monotonic(LogindContext *c);
extern gboolean
logind_set_brightness(LogindContext *c, const char *sys, const char *name,
        guint32 v);
extern LogindContext *
logind_context_new(void);
extern void
logind_context_free(LogindContext *c);
