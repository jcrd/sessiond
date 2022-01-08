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

#pragma once

#include <glib-2.0/glib.h>

#define IDLE_TABLE_LIST \
    X("Inputs", input_mask, input_mask) \
    X("IdleSec", uint, idle_sec)

#define LOCK_TABLE_LIST \
    X("OnIdle", bool, on_idle) \
    X("OnSleep", bool, on_sleep)

#define BACKLIGHT_TABLE_LIST \
    X("DimSec", uint, dim_sec) \
    X("DimValue", int, dim_value) \
    X("DimPercent", double, dim_percent)

#ifdef DPMS
#define DPMS_TABLE_LIST \
    X("Enable", bool, dpms_enable) \
    X("StandbySec", uint, standby_sec) \
    X("SuspendSec", uint, suspend_sec) \
    X("OffSec", uint, off_sec)

#define DPMS_LOCK_TABLE_LIST \
    X("StandbySec", uint, lock_standby_sec) \
    X("SuspendSec", uint, lock_suspend_sec) \
    X("OffSec", uint, lock_off_sec)
#endif /* DPMS */

#ifdef WIREPLUMBER
#define WP_LOCK_TABLE_LIST \
    X("MuteAudio", bool, mute_audio)
#endif /* WIREPLUMBER */

struct BacklightConf {
    guint dim_sec;
    gint dim_value;
    gdouble dim_percent;
};

typedef struct {
    /* Idle */
    guint input_mask;
    guint idle_sec;
    /* Lock */
    gboolean on_idle;
    gboolean on_sleep;
    /* Backlights */
    GHashTable *backlights;
    /* Hooks */
    GPtrArray *hooks;
#ifdef DPMS
    /* DPMS */
    gboolean dpms_enable;
    guint standby_sec;
    guint suspend_sec;
    guint off_sec;

    guint lock_standby_sec;
    guint lock_suspend_sec;
    guint lock_off_sec;
#endif /* DPMS */
#ifdef WIREPLUMBER
    /* WIREPLUMBER */
    gboolean mute_audio;
#endif /* WIREPLUMBER */
} Config;

extern Config
config_new(void);
extern gboolean
config_load(const gchar *path, const gchar *hooksd, Config *c);
extern void
config_free(Config *c);
