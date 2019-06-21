#pragma once

#include "dbus-logind.h"
#include "dbus-gen.h"

#include <glib-2.0/glib.h>

#define DBUS_TYPE_SERVER dbus_server_get_type()
G_DECLARE_FINAL_TYPE(DBusServer, dbus_server, DBUS, SERVER, GObject);

struct _DBusServer {
    GObject parent;
    gboolean exported;
    guint bus_id;
    DBusSession *session;
    LogindContext *ctx;
    GHashTable *inhibitors;
};

extern void
dbus_server_free(DBusServer *s);
extern void
dbus_server_emit_active(DBusServer *s);
extern void
dbus_server_emit_inactive(DBusServer *s, guint i);
extern DBusServer *
dbus_server_new(LogindContext *c);
