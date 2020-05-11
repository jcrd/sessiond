/*
sessiond - standalone X session manager
Copyright (C) 2018-2020 James Reed

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

#include "hooks.h"
#include "common.h"
#include "timeline.h"

#include <glib-2.0/glib.h>

static void
run_hooks_timeout(GPtrArray *hooks, HookTrigger trigger, gboolean state,
                  guint timeout)
{
    for (guint i = 0; i < hooks->len; i++) {
        struct Hook *h = g_ptr_array_index(hooks, i);
        if (h->trigger != trigger
            || (trigger == HOOK_TRIGGER_INACTIVE
                && h->inactive_sec != timeout))
            continue;
        if (state && h->exec_start)
            spawn_exec(h->exec_start);
        else if (!state && h->exec_stop)
            spawn_exec(h->exec_stop);
    }
}

void
hooks_add_timeouts(GPtrArray *hooks, Timeline *tl)
{
    for (guint i = 0; i < hooks->len; i++) {
        struct Hook *h = g_ptr_array_index(hooks, i);
        if (h->trigger == HOOK_TRIGGER_INACTIVE)
            timeline_add_timeout(tl, h->inactive_sec);
    }
}

void
hooks_run(GPtrArray *hooks, HookTrigger trigger, gboolean state)
{
    if (trigger == HOOK_TRIGGER_INACTIVE)
        return;
    run_hooks_timeout(hooks, trigger, state, 0);
}

void
hooks_on_timeout(GPtrArray *hooks, guint timeout, gboolean state)
{
    run_hooks_timeout(hooks, HOOK_TRIGGER_INACTIVE, state, timeout);
}
