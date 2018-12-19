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

#include "configure.h"

#define CONFIG_MAP_LIST \
    X("Idle", "Input", input_mask, input_mask) \
    X("Idle", "IdleSec", uint, idle_sec) \
    X("Lock", "OnIdle", bool, on_idle) \
    X("Lock", "OnSleep", bool, on_sleep) \
    X("Backlight", "Enable", bool, bl_enable) \
    X("Backlight", "Name", str, bl_name) \
    X("Backlight", "DimSec", uint, dim_sec) \
    X("Backlight", "Percent", uint, dim_percent)

#ifdef DPMS
#define DPMS_MAP_LIST \
    X("DPMS", "Enable", bool, dpms_enable) \
    X("DPMS", "StandbySec", uint, standby_sec) \
    X("DPMS", "SuspendSec", uint, suspend_sec) \
    X("DPMS", "OffSec", uint, off_sec)
#endif /* DPMS */

typedef struct {
    /* Idle */
    guint input_mask;
    guint idle_sec;
    /* Lock */
    gboolean on_idle;
    gboolean on_sleep;
    /* Backlight */
    gboolean bl_enable;
    gchar *bl_name;
    guint dim_sec;
    guint dim_percent;
#ifdef DPMS
    /* DPMS */
    gboolean dpms_enable;
    guint standby_sec;
    guint suspend_sec;
    guint off_sec;
#endif /* DPMS */
} Config;

extern gboolean
config_load(Config *c, const gchar *path);
extern void
config_free(Config *c);
