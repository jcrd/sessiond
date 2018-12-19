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

typedef void (*TimelineFunc)(guint timeout, gboolean state);

typedef struct {
    gboolean running;
    GMainContext *ctx;
    GSource *source;
    GArray *timeouts;
    guint index;
    gint64 inactive_since;
    TimelineFunc func;
} Timeline;

extern Timeline
timeline_new(GMainContext *ctx, TimelineFunc func);
extern gboolean
timeline_add_timeout(Timeline *tl, guint timeout);
extern void
timeline_start(Timeline *tl);
extern void
timeline_stop(Timeline *tl);
extern void
timeline_free(Timeline *tl);
