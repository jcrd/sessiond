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

#define G_LOG_DOMAIN "sessiond"

#include "backlight.h"
#include "common.h"
#include "config.h"
#include "dbus-logind.h"
#include "dbus-server.h"
#include "dbus-systemd.h"
#include "hooks.h"
#include "timeline.h"
#include "xsource.h"
#include "version.h"

#include <locale.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>
#include <glib-2.0/glib-unix.h>
#include <glib-2.0/glib/gstdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#ifdef DPMS
#include <X11/extensions/dpms.h>
#endif /* DPMS */

static Config config;
static Timeline timeline;
static LogindContext *lc = NULL;
static SystemdContext *sc = NULL;
static DBusServer *server = NULL;
static XSource *xsource = NULL;
static gboolean inhibited = FALSE;
static gboolean inactive = FALSE;
static GMainLoop *main_loop = NULL;
static GMainContext *main_ctx = NULL;
static GPtrArray *hooks = NULL;
static gchar *bl_iface = NULL;
static gint bl_value = -1;
static gchar *config_path = NULL;
static guint idle_sec = 0;
#ifdef DPMS
static gboolean no_dpms = FALSE;
#endif /* DPMS */

static void
set_idle(gboolean state)
{
    if (state == logind_get_idle_hint(lc))
        return;

    logind_set_idle_hint(lc, state);

    if (state) {
        systemd_start_unit(sc, "graphical-idle.target");
        if (config.on_idle && !logind_get_locked_hint(lc))
            logind_lock_session(lc, TRUE);
    } else {
        systemd_start_unit(sc, "graphical-unidle.target");
    }

    if (hooks)
        hooks_run(hooks, HOOK_TYPE_IDLE, state);

    g_message("Idle: %s", BOOLSTR(state));
}

static void
lock_callback(LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state == logind_get_locked_hint(c))
        return;

    logind_set_locked_hint(c, state);

    if (state) {
        systemd_start_unit(sc, "graphical-lock.target");
    } else {
        timeline_start(&timeline);
        systemd_start_unit(sc, "graphical-unlock.target");
    }

    if (hooks)
        hooks_run(hooks, HOOK_TYPE_LOCK, state);

    g_message("Locked: %s", BOOLSTR(state));
}

static void
sleep_callback(LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state) {
        g_message("Preparing for sleep...");
        systemd_start_unit(sc, "user-sleep.target");
        if (config.on_sleep && !logind_get_locked_hint(c))
            logind_lock_session(c, TRUE);
    }

    if (hooks)
        hooks_run(hooks, HOOK_TYPE_SLEEP, state);
}

static void
shutdown_callback(UNUSED LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state) {
        g_message("Preparing for shutdown...");
        systemd_start_unit(sc, "user-shutdown.target");
    }

    if (hooks)
        hooks_run(hooks, HOOK_TYPE_SHUTDOWN, state);
}

static void
inhibit_callback(UNUSED DBusServer *s, const gchar *who, const gchar *why,
        UNUSED guint n, UNUSED gpointer data)
{
    g_message("Inhibitor added: %s (%s)", who, why);

    inhibited = TRUE;
    timeline_stop(&timeline);
}

static void
uninhibit_callback(UNUSED DBusServer *s, const gchar *who, const gchar *why,
        guint n, UNUSED gpointer data)
{
    g_message("Inhibitor removed: %s (%s)", who, why);

    if (n > 0)
        return;
    inhibited = FALSE;
    timeline_start(&timeline);
}

static void
appear_callback(UNUSED LogindContext *c, UNUSED gpointer data)
{
    if (!server) {
        server = dbus_server_new(lc);
        g_signal_connect(server, "inhibit", G_CALLBACK(inhibit_callback),
                NULL);
        g_signal_connect(server, "uninhibit", G_CALLBACK(uninhibit_callback),
                NULL);
    }
}

static void
vanish_callback(UNUSED LogindContext *c, UNUSED gpointer data)
{
    dbus_server_free(server);
    server = NULL;
}

#ifdef DPMS
static gboolean
set_dpms(Config c)
{
    int event;
    int error;
    if (!DPMSQueryExtension(xsource->dpy, &event, &error)) {
        g_warning("DPMS extension is not available");
        return FALSE;
    }

    if (!DPMSCapable(xsource->dpy)) {
        g_warning("X server is not capable of DPMS");
        return FALSE;
    }

    if (c.dpms_enable) {
        DPMSEnable(xsource->dpy);

        Status status = DPMSSetTimeouts(xsource->dpy, (CARD16)c.standby_sec,
                                        (CARD16)c.suspend_sec,
                                        (CARD16)c.off_sec);

        if (status == BadValue) {
            g_warning("Inconsistent DPMS timeouts supplied; \
see sessiond.conf(5) for more details");
            return FALSE;
        }

        g_debug("DPMS timeouts set (standby: %u, suspend: %u, off: %u)",
                c.standby_sec, c.suspend_sec, c.off_sec);
    } else {
        /* support config reloading */
        DPMSDisable(xsource->dpy);
        g_debug("DPMS disabled");
    }

    return TRUE;
}
#endif /* DPMS */

static gboolean
xsource_cb(UNUSED gpointer user_data)
{
    if (!xsource->connected) {
        g_critical("X connection lost");
        g_main_loop_quit(main_loop);
        return G_SOURCE_REMOVE;
    }

    if (!inhibited)
        timeline_start(&timeline);

    return G_SOURCE_CONTINUE;
}

static gboolean
quit_signal(UNUSED gpointer user_data)
{
    g_main_loop_quit(main_loop);
    return TRUE;
}

static void
set_backlight(gboolean state)
{
    if (state) {
        if ((bl_value = backlight_get(bl_iface)) != -1) {
            guint dim = bl_value - bl_value * config.dim_percent / 100;
            if (!dim)
                return;
            g_debug("Backlight value is %i", bl_value);
            backlight_set(bl_iface, dim);
        }
    } else if (bl_value != -1) {
        backlight_set(bl_iface, bl_value);
        bl_value = -1;
    }
}

static void
on_timeout(guint timeout, gboolean state, gconstpointer user_data)
{
    if (timeout == config.idle_sec)
        set_idle(state);
    else if (bl_iface && timeout == config.dim_sec)
        set_backlight(state);

    if (hooks)
        hooks_on_timeout(hooks, timeout, state);

    if (state)
        dbus_server_emit_inactive(server, timeout);
    else if (inactive)
        dbus_server_emit_active(server);

    inactive = state;
}

static void
load_hook_path(const gchar *path, GPtrArray **hooks)
{
    GPtrArray *hs = NULL;
    gchar *p = g_strjoin("/", path, "hooks.d", NULL);

    if (g_file_test(p, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
        if ((hs = hooks_load(p))) {
            *hooks = hs;
            g_info("Loaded hooks at %s", p);
        }
    } else {
        g_debug("No hooks directory found at %s", p);
    }

    g_free(p);
}

static gboolean
load_config_path(const gchar *path, Config *config)
{
    Config c;
    gboolean ok = FALSE;

    if (config_path) {
        if (g_file_test(config_path, G_FILE_TEST_EXISTS))
            ok = config_load(&c, config_path);
        else
            g_critical("No config file found at %s", config_path);
    } else {
        gchar *p = g_strjoin("/", path, "sessiond.conf", NULL);
        if (g_file_test(p, G_FILE_TEST_EXISTS)) {
            if ((ok = config_load(&c, p)))
                g_info("Loaded config file at %s", p);
        } else {
            g_debug("No config file found at %s", p);
        }

        g_free(p);
    }

    if (ok) {
        if (idle_sec)
            c.idle_sec = idle_sec;
        *config = c;
    }

    return ok;
}

static gboolean
load_files(Config *config, GPtrArray **hooks)
{
    const gchar *dir = g_get_user_config_dir();
    gchar *path = dir ? g_strjoin("/", dir, "sessiond", NULL) :
        g_strjoin("/", getenv("HOME"), ".config", "sessiond", NULL);

    if (!load_config_path(path, config)) {
        g_free(path);
        return FALSE;
    }

    load_hook_path(path, hooks);

    g_free(path);

    return TRUE;
}

static void
init_backlight(void)
{
    if (config.bl_enable)
        bl_iface = backlight_get_interface(config.bl_name);
}

static void
init_timeline(Timeline *tl)
{
    *tl = timeline_new(main_ctx, on_timeout, NULL);

    timeline_add_timeout(tl, config.idle_sec);

    if (bl_iface)
        timeline_add_timeout(tl, config.dim_sec);

    if (hooks)
        hooks_add_timeouts(hooks, tl);

    timeline_start(tl);
}

static gboolean
init_xsource(XSource **source)
{
    XSource *s = xsource_new(main_ctx, config.input_mask, xsource_cb, NULL,
                             NULL);
    if (s)
        *source = s;

    return s != NULL;
}

static void
init_dbus(void)
{
    if (!lc) {
        lc = logind_context_new();
        g_signal_connect(lc, "lock", G_CALLBACK(lock_callback), NULL);
        g_signal_connect(lc, "sleep", G_CALLBACK(sleep_callback), NULL);
        g_signal_connect(lc, "shutdown", G_CALLBACK(shutdown_callback), NULL);
        g_signal_connect(lc, "appear", G_CALLBACK(appear_callback), NULL);
        g_signal_connect(lc, "vanish", G_CALLBACK(vanish_callback), NULL);
    }

    if (!sc)
        sc = systemd_context_new();
}

static gboolean
reload_signal(UNUSED gpointer user_data)
{
    Config c;
    GPtrArray *hs = NULL;

    if (load_files(&c, &hs)) {
        g_message("Reloading configuration files...");

        XSource *s;
        if (c.input_mask != config.input_mask && init_xsource(&s)) {
            xsource_free(xsource);
            xsource = s;
        }

        config = c;
        g_free(bl_iface);
        bl_iface = NULL;
        init_backlight();
        hooks_free(hooks);
        hooks = hs;
        timeline_free(&timeline);
        init_timeline(&timeline);
        init_dbus();
#ifdef DPMS
        if (!no_dpms)
            set_dpms(config);
#endif /* DPMS */
    } else {
        g_message("Nothing to reload");
    }

    return TRUE;
}

static void
cleanup(void)
{
    g_free(bl_iface);
    g_free(config_path);
    hooks_free(hooks);
    timeline_free(&timeline);
    logind_context_free(lc);
    systemd_context_free(sc);
    dbus_server_free(server);
    xsource_free(xsource);
    g_main_context_unref(main_ctx);
    g_main_loop_unref(main_loop);
}

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    g_set_application_name("sessiond");

    gboolean version = FALSE;
    GOptionEntry opts[] = {
        {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path,
         "Path to config file", "CONFIG"},
        {"idle-sec", 'i', 0, G_OPTION_ARG_INT, &idle_sec,
         "Seconds the session must be inactive before considered idle", "SEC"},
        {"version", 'v', 0, G_OPTION_ARG_NONE, &version, "Show version", NULL},
        {NULL},
    };

    GOptionContext *ctx = g_option_context_new("- standalone X session manager");
    g_option_context_add_main_entries(ctx, opts, NULL);

    GError *err = NULL;
    g_option_context_parse(ctx, &argc, &argv, &err);
    g_option_context_free(ctx);

    if (err) {
        g_critical("%s", err->message);
        g_error_free(err);
        return EXIT_FAILURE;
    }

    if (version) {
        printf("%s\n", VERSION);
        return EXIT_SUCCESS;
    }

    atexit(cleanup);

    if (!load_files(&config, &hooks))
        return EXIT_FAILURE;

    init_backlight();

    main_loop = g_main_loop_new(NULL, FALSE);
    main_ctx = g_main_loop_get_context(main_loop);

    if (!init_xsource(&xsource))
        return EXIT_FAILURE;

    init_dbus();

#ifdef DPMS
    set_dpms(config);
#endif /* DPMS */

    init_timeline(&timeline);

    g_unix_signal_add(SIGINT, quit_signal, NULL);
    g_unix_signal_add(SIGTERM, quit_signal, NULL);
    g_unix_signal_add(SIGHUP, reload_signal, NULL);

    g_main_loop_run(main_loop);
}
