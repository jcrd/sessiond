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

extern gboolean
kf_load_bool(GKeyFile *kf, const gchar *g, const gchar *k, gboolean *opt);
extern gboolean
kf_load_str(GKeyFile *kf, const gchar *g, const gchar *k, gchar **opt);
extern gboolean
kf_load_int(GKeyFile *kf, const gchar *g, const gchar *k, gint *opt);
extern gboolean
kf_load_uint(GKeyFile *kf, const gchar *g, const gchar *k, guint *opt);
