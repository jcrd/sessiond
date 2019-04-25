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

typedef void (*SignalFunc)(gboolean state);

typedef struct {
    gchar *session_id;
    gboolean idle_hint;
    gboolean locked_hint;
    guint logind_watcher;
    GDBusProxy *logind_session;
    GDBusProxy *logind_manager;
    SignalFunc logind_lock_func;
    SignalFunc logind_sleep_func;
    SignalFunc logind_shutdown_func;
} LogindContext;

extern void
logind_set_idle_hint(LogindContext *c, gboolean state);
extern void
logind_lock_session(LogindContext *c, gboolean state);
extern void
logind_set_locked_hint(LogindContext *c, gboolean state);
extern LogindContext *
logind_new(void);
extern void
logind_free(LogindContext *c);
