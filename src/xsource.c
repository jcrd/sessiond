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

#include "xsource.h"
#include "common.h"

#include <glib-2.0/glib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#define XI_MAJOR_VERSION 2
#define XI_MINOR_VERSION 0

static gboolean
xsource_prepare(GSource *source, gint *timeout)
{
    XSource *self = (XSource *)source;
    XSync(self->dpy, FALSE);
    *timeout = -1;
    return FALSE;
}

static gboolean
xsource_check(GSource *source)
{
    XSource *self = (XSource *)source;
    GIOCondition revents = g_source_query_unix_fd(source, self->fd);

    if (revents & (G_IO_HUP | G_IO_ERR)) {
        self->connected = FALSE;
        return TRUE;
    }

    if (!(revents & G_IO_IN))
        return FALSE;

    guint events = 0;

    while (XPending(self->dpy)) {
        XEvent ev;
        XNextEvent(self->dpy, &ev);

        if (ev.type != GenericEvent)
            continue;

        XGenericEventCookie c = ev.xcookie;
        XGetEventData(self->dpy, &c);
        XIDeviceEvent *iev = (XIDeviceEvent *)c.data;

        switch (iev->evtype) {
#define X(t, n) \
            case XI_##t: \
                events++; \
                g_debug("%s @ %lu", n, iev->time); \
                break;
            INPUT_TYPE_LIST
#undef X
        }

        XFreeEventData(self->dpy, &c);
    }

    return events > 0;
}

static gboolean
xsource_dispatch(UNUSED GSource *source, GSourceFunc func, gpointer user_data)
{
    return func(user_data);
}

static void
xsource_finalize(GSource *source)
{
    XSource *self = (XSource *)source;
    self->connected = FALSE;
}

static GSourceFuncs xsource_funcs = {
    xsource_prepare,
    xsource_check,
    xsource_dispatch,
    xsource_finalize,
    NULL,
    NULL,
};

XSource *
xsource_new(GMainContext *ctx, guint input_mask, GSourceFunc func,
            gpointer user_data, GDestroyNotify destroy)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        g_critical("Failed to open X display");
        return NULL;
    }

    int opcode, event, error;
    if (!XQueryExtension(dpy, "XInputExtension", &opcode, &event, &error)) {
        g_critical("XInputExtension is not available");
        return NULL;
    }

    int major = XI_MAJOR_VERSION;
    int minor = XI_MINOR_VERSION;
    if (XIQueryVersion(dpy, &major, &minor) != Success) {
        g_critical("XInputExtension %i.%i is not supported", major, minor);
        return NULL;
    }

    XIEventMask evmask;
    evmask.deviceid = XIAllMasterDevices;
    evmask.mask_len = XIMaskLen(XI_LASTEVENT);
    evmask.mask = g_malloc0_n(evmask.mask_len, sizeof(char));

#define X(type, _) \
    if (input_mask & INPUT_TYPE_MASK(type)) \
        XISetMask(evmask.mask, XI_##type);
    INPUT_TYPE_LIST
#undef X

    XISelectEvents(dpy, DefaultRootWindow(dpy), &evmask, 1);
    free(evmask.mask);

    GSource *source = g_source_new(&xsource_funcs, sizeof(XSource));
    XSource *self = (XSource *)source;
    self->dpy = dpy;
    self->fd = g_source_add_unix_fd(source, ConnectionNumber(dpy),
                                    G_IO_IN | G_IO_HUP | G_IO_ERR);
    self->connected = TRUE;

    g_source_set_callback(source, func, user_data, destroy);
    g_source_attach(source, ctx);

    return self;
}

void
xsource_free(XSource *self)
{
    if (!self)
        return;

    XCloseDisplay(self->dpy);

    GSource *source = (GSource *)self;
    g_source_destroy(source);
    g_source_unref(source);
}
