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

#include "dbus-logind.h"

#include <glib-2.0/glib.h>
#include <libudev.h>

#define BL_ACTION_LIST \
    X(ADD, "add") \
    X(REMOVE, "remove") \
    X(CHANGE, "change") \
    X(ONLINE, "online") \
    X(OFFLINE, "offline")

typedef enum {
#define X(action, _) BL_ACTION_##action,
    BL_ACTION_LIST
#undef X
} BacklightAction;

typedef enum {
    BL_TYPE_DISPLAY,
    BL_TYPE_KEYBOARD,
} BacklightType;

struct Backlight {
    struct udev_device *device;
    const char *name;
    const char *subsystem;
    gchar *sys_path;
    const char *dev_path;
    gboolean online;
    gint brightness;
    gint max_brightness;
    gint pre_dim_brightness;
};

typedef gboolean (*BacklightsFunc)(BacklightAction a, const gchar *path,
        struct Backlight *bl);

typedef struct {
    GSource source;
    gpointer fd;
    struct udev *udev;
    struct udev_monitor *udev_mon;
    GQueue *queue;
    GHashTable *devices;
} Backlights;

extern Backlights *
backlights_new(GMainContext *ctx, BacklightsFunc func);
extern void
backlights_free(Backlights *bls);
extern gchar *
backlight_normalize_name(const char *name);
extern gboolean
backlight_set_brightness(struct Backlight *bl, guint32 v, LogindContext *ctx);
extern void
backlights_on_timeout(GHashTable *devs, GHashTable *cs, guint timeout,
        gboolean state, LogindContext *ctx);
