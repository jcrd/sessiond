/*
sessiond - standalone X session manager
Copyright (C) 2018-2019 James Reed

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

#define HOOKS_TABLE_LIST \
    X("Trigger", trigger, trigger) \
    X("InactiveSec", uint, inactive_sec) \
    X("ExecStart", exec, exec_start) \
    X("ExecStop", exec, exec_stop)

#define HOOK_TRIGGER_LIST \
    X(LOCK, "Lock") \
    X(IDLE, "Idle") \
    X(SLEEP, "Sleep") \
    X(SHUTDOWN, "Shutdown") \
    X(INACTIVE, "Inactive")

typedef enum {
    HOOK_TRIGGER_NONE,
#define X(trigger, _) HOOK_TRIGGER_##trigger,
    HOOK_TRIGGER_LIST
#undef X
} HookTrigger;

struct Hook {
    HookTrigger trigger;
    guint inactive_sec;
    gchar **exec_start;
    gchar **exec_stop;
};

extern void
hooks_add_timeouts(GPtrArray *hooks, Timeline *tl);
extern void
hooks_run(GPtrArray *hooks, HookTrigger trigger, gboolean state);
extern void
hooks_on_timeout(GPtrArray *hooks, guint timeout, gboolean state);
