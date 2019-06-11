#pragma once

#include "dbus-logind.h"
#include "dbus-gen.h"

#include <glib-2.0/glib.h>

typedef struct {
    guint bus_id;
    DBusSession *session;
    LogindContext *ctx;
} DBusServer;

extern void
dbus_server_free(DBusServer *s);
extern void
dbus_server_emit_active(DBusServer *s);
extern void
dbus_server_emit_inactive(DBusServer *s, guint i);
extern DBusServer *
dbus_server_init(LogindContext *c);
