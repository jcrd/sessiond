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

#include "timeline.h"

#include <glib-2.0/glib.h>

#define HOOK_TYPE_LIST \
    X(LOCK, "Lock") \
    X(IDLE, "Idle") \
    X(SLEEP, "Sleep") \
    X(SHUTDOWN, "Shutdown") \
    X(INACTIVE, "Inactive")

typedef enum {
    HOOK_TYPE_NONE,
#define X(type, _) HOOK_TYPE_##type,
    HOOK_TYPE_LIST
#undef X
} HookType;

typedef struct {
    HookType type;
    guint inactive_sec;
    gchar **exec_start;
    gchar **exec_stop;
} Hook;

extern GPtrArray *
hooks_load(const gchar *path);
extern void
hooks_add_timeouts(GPtrArray *hooks, Timeline *tl);
extern void
hooks_run(GPtrArray *hooks, HookType type, gboolean state);
extern void
hooks_on_timeout(GPtrArray *hooks, guint timeout, gboolean state);
extern void
hooks_free(GPtrArray *hooks);
