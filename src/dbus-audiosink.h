/*
sessiond - standalone X session manager
Copyright (C) 2021 James Reed

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
#include "wireplumber.h"

#include <glib-2.0/glib.h>

#define DBUS_AUDIOSINK_ERROR DBUS_NAME ".AudioSink.Error"
#define DBUS_AUDIOSINK_PATH DBUS_PATH "/audiosink"

extern gboolean
dbus_server_export_audiosink(DBusServer *s, DBusAudioSink *das);
extern void
dbus_server_unexport_audiosink(DBusServer *s, DBusAudioSink *das);
extern void
dbus_server_add_audiosink(DBusServer *s, struct AudioSink *as);
extern void
dbus_server_remove_audiosink(DBusServer *s, guint32 id);
extern void
dbus_server_update_audiosink(DBusServer *s, struct AudioSink *as);
extern void
dbus_server_update_default_audiosink(DBusServer *s, guint32 id);
