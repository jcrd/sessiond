/*
sessiond - standalone X session manager
Copyright (C) 2019-2020 James Reed

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

#include "dbus-server.h"

#include <glib-2.0/glib.h>

#define DBUS_BACKLIGHT_ERROR DBUS_NAME ".Backlight.Error"
#define DBUS_BACKLIGHT_PATH DBUS_PATH "/backlight"

extern gboolean
dbus_server_export_backlight(DBusServer *s, DBusBacklight *dbl);
extern void
dbus_server_unexport_backlight(DBusServer *s, DBusBacklight *dbl);
extern void
dbus_server_add_backlight(DBusServer *s, struct Backlight *bl);
extern void
dbus_server_remove_backlight(DBusServer *s, const char *path);
extern void
dbus_server_update_backlight(DBusServer *s, struct Backlight *bl);
