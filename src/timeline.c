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

#define G_LOG_DOMAIN "sessiond-activity"

#include "timeline.h"

#include <glib-2.0/glib.h>

static void
add_timeout(Timeline *tl, guint timeout);

static gint
compare_timeouts(gconstpointer a, gconstpointer b)
{
    guint time_a = *(guint *)a;
    guint time_b = *(guint *)b;

    if (time_a < time_b)
        return -1;
    if (time_a > time_b)
        return 1;

    return 0;
}

static guint
get_timeout(Timeline *tl)
{
    return g_array_index(tl->timeouts, guint, tl->index);
}

static gboolean
on_timeout(gpointer user_data)
{
    Timeline *tl = (Timeline *)user_data;

    if (!tl || !tl->running)
        return G_SOURCE_REMOVE;

    guint inactive = (g_get_monotonic_time() - tl->inactive_since) / 1000000;
    guint timeout = get_timeout(tl);

    if (inactive >= timeout) {
        tl->index++;
        tl->func(timeout, TRUE, tl->user_data);
        if (tl->index < tl->timeouts->len)
            add_timeout(tl, get_timeout(tl) - inactive);
        else
            timeline_stop(tl);
    } else {
        add_timeout(tl, timeout - inactive);
    }

    return G_SOURCE_REMOVE;
}

static void
remove_source(Timeline *tl)
{
    if (!tl->source)
        return;
    g_source_destroy(tl->source);
    g_source_unref(tl->source);
    tl->source = NULL;
}

static void
add_timeout(Timeline *tl, guint timeout)
{
    remove_source(tl);

    tl->source = g_timeout_source_new_seconds(timeout);
    g_source_set_callback(tl->source, on_timeout, tl, NULL);
    g_source_attach(tl->source, tl->ctx);

    g_debug("%us timeout added", timeout);
}

Timeline
timeline_new(GMainContext *ctx, TimelineFunc func, gconstpointer user_data)
{
    Timeline tl;
    tl.running = FALSE;
    tl.ctx = ctx;
    tl.source = NULL;
    tl.timeouts = g_array_new(FALSE, FALSE, sizeof(guint));
    tl.index = 0;
    tl.inactive_since = -1;
    tl.func = func;
    tl.user_data = user_data;
    return tl;
}

gboolean
timeline_add_timeout(Timeline *tl, guint timeout)
{
    for (guint i = 0; i < tl->timeouts->len; i++)
        if (g_array_index(tl->timeouts, guint, i) == timeout)
            return FALSE;

    g_array_append_val(tl->timeouts, timeout);

    return TRUE;
}

void
timeline_start(Timeline *tl)
{
    if (tl->running) {
        while (tl->index) {
            tl->index--;
            tl->func(get_timeout(tl), FALSE, tl->user_data);
        }
    } else if (tl->timeouts->len > 0) {
        g_array_sort(tl->timeouts, compare_timeouts);
        tl->running = TRUE;
    } else {
        return;
    }

    tl->index = 0;
    add_timeout(tl, get_timeout(tl));
    tl->inactive_since = g_get_monotonic_time();
}

void
timeline_stop(Timeline *tl)
{
    if (!tl->running)
        return;
    if (!tl->index)
        tl->running = FALSE;
    tl->inactive_since = -1;
    remove_source(tl);
}

guint
timeline_pending_timeouts(Timeline *tl)
{
    return tl->timeouts->len - tl->index;
}

void
timeline_free(Timeline *tl)
{
    if (!tl)
        return;
    timeline_stop(tl);
    if (tl->timeouts)
        g_array_free(tl->timeouts, TRUE);
}
