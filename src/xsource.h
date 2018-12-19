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
#include <X11/Xlib.h>

#define INPUT_TYPE_LIST \
    X(RawMotion, "motion") \
    X(RawButtonPress, "button-press") \
    X(RawButtonRelease, "button-release") \
    X(RawKeyPress, "key-press") \
    X(RawKeyRelease, "key-release")

#define INPUT_TYPE_MASK(type) (1 << INPUT_TYPE_##type)

enum {
#define X(type, _) INPUT_TYPE_##type,
    INPUT_TYPE_LIST
#undef X
    INPUT_TYPE_COUNT
};

typedef struct {
    GSource source;
    Display *dpy;
    gpointer fd;
    gboolean connected;
} XSource;

extern XSource *
xsource_new(GMainContext *ctx, guint input_mask, GSourceFunc func,
            gpointer user_data, GDestroyNotify destroy);
extern void
xsource_free(XSource *self);
