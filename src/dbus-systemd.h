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
#include <glib-2.0/gio/gio.h>

typedef struct {
    guint systemd_watcher;
    GDBusProxy *systemd_manager;
} SystemdContext;

extern void
systemd_start_unit(SystemdContext *c, const gchar *name);
extern SystemdContext *
systemd_context_new(void);
extern void
systemd_context_free(SystemdContext *c);
