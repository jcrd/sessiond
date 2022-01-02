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

#include "dbus-logind.h"
#include "dbus-gen.h"
#include "backlight.h"

#ifdef WIREPLUMBER
#include "wireplumber.h"
#endif /* WIREPLUMBER */

#include <glib-2.0/glib.h>

#define DBUS_NAME "org.sessiond.session1"
#define DBUS_SESSION_ERROR DBUS_NAME ".Session.Error"
#define DBUS_PATH "/org/sessiond/session1"

#define DBUS_TYPE_SERVER dbus_server_get_type()
G_DECLARE_FINAL_TYPE(DBusServer, dbus_server, DBUS, SERVER, GObject);

struct _DBusServer {
    GObject parent;
    guint bus_id;
    GDBusConnection *conn;
    gboolean name_acquired;
    DBusSession *session;
    LogindContext *ctx;
    GHashTable *inhibitors;
    GHashTable *backlights;
    GHashTable *bl_devices;

#ifdef WIREPLUMBER
    WpConn *wp_conn;
    GHashTable *audiosinks;
#endif /* WIREPLUMBER */
};

extern void
dbus_server_free(DBusServer *s);
extern void
dbus_server_emit_active(DBusServer *s);
extern void
dbus_server_emit_inactive(DBusServer *s, guint i);
extern DBusServer *
dbus_server_new(LogindContext *c);
