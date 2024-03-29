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

#define G_LOG_DOMAIN "sessiond"

#include "backlight.h"
#include "common.h"
#include "config.h"
#include "dbus-logind.h"
#include "dbus-server.h"
#include "dbus-backlight.h"
#include "dbus-systemd.h"
#include "hooks.h"
#include "timeline.h"
#include "xsource.h"
#include "version.h"

#ifdef WIREPLUMBER
#include "dbus-audiosink.h"
#include "wireplumber.h"
#endif /* WIREPLUMBER */

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
static LogindContext *logind_ctx = NULL;
static SystemdContext *systemd_ctx = NULL;
static DBusServer *server = NULL;
static XSource *xsource = NULL;
static gboolean inhibited = FALSE;
static gboolean inactive = FALSE;
static gboolean idle = FALSE;
static GMainLoop *main_loop = NULL;
static GMainContext *main_ctx = NULL;
static Backlights *backlights = NULL;
static gchar *config_path = NULL;
static gchar *hooksd_path = NULL;
static guint idle_sec = 0;
#ifdef DPMS
static gboolean no_dpms = FALSE;

static gboolean
set_dpms_timeouts(guint standby, guint suspend, guint off)
{
    Status status = DPMSSetTimeouts(xsource->dpy, (CARD16)standby,
                                        (CARD16)suspend,
                                        (CARD16)off);

    if (status == BadValue) {
        g_warning("Inconsistent DPMS timeouts supplied; \
see sessiond.conf(5) for more details");
        return FALSE;
    }

    g_debug("DPMS timeouts set (standby: %u, suspend: %u, off: %u)",
            standby, suspend, off);

    return TRUE;
}

static gboolean
set_dpms(const Config *c)
{
    int event;
    int error;
    if (!DPMSQueryExtension(xsource->dpy, &event, &error)) {
        g_warning("DPMS extension is not available");
        goto err;
    }

    if (!DPMSCapable(xsource->dpy)) {
        g_warning("X server is not capable of DPMS");
        goto err;
    }

    if (c->dpms_enable) {
        DPMSEnable(xsource->dpy);
        return set_dpms_timeouts(c->standby_sec, c->suspend_sec, c->off_sec);
    } else {
        /* support config reloading */
        DPMSDisable(xsource->dpy);
        g_debug("DPMS disabled");
        return TRUE;
    }

err:
    no_dpms = TRUE;
    return FALSE;
}

static void
set_dpms_locked(gboolean state)
{
    const Config *c = &config;

    if (no_dpms || !c->dpms_enable)
        return;

    if (state)
        set_dpms_timeouts(c->lock_standby_sec, c->lock_suspend_sec,
                c->lock_off_sec);
    else
        set_dpms_timeouts(c->standby_sec, c->suspend_sec, c->off_sec);
}
#endif /* DPMS */

#ifdef WIREPLUMBER
static void
wireplumber_cb(AudioSinkAction a, guint32 id, struct AudioSink *as)
{
    switch (a) {
        case AS_ACTION_ADD:
            dbus_server_add_audiosink(server, as);
            break;
        case AS_ACTION_REMOVE:
            dbus_server_remove_audiosink(server, id);
            break;
        case AS_ACTION_CHANGE:
            dbus_server_update_audiosink(server, as);
            break;
        case AS_ACTION_CHANGE_DEFAULT:
            dbus_server_update_default_audiosink(server, id);
            break;
    }
}

static void
mute_audio_locked(gboolean state)
{
    static guint32 default_id = 0;
    static gboolean default_mute = TRUE;

    if (!config.mute_audio)
        return;

    WpConn *conn = server->wp_conn;

    if (state) {
        default_id = conn->default_id;
        audiosink_get_volume_mute(conn, default_id, NULL, &default_mute);
        audiosink_set_mute(default_id, TRUE, conn);
    } else if (conn->default_id == default_id) {
        audiosink_set_mute(default_id, default_mute, conn);
    } else {
        default_id = 0;
        default_mute = TRUE;
    }
}
#endif /* WIREPLUMBER */

static void
set_idle(gboolean state)
{
    if (state == idle)
        return;

    idle = state;

    logind_set_idle_hint(logind_ctx, state);

    if (state) {
        systemd_start_unit(systemd_ctx, "graphical-idle.target");
        if (config.on_idle && !logind_get_locked_hint(logind_ctx))
            logind_lock_session(logind_ctx, TRUE);
    } else {
        timeline_start(&timeline);
        systemd_start_unit(systemd_ctx, "graphical-unidle.target");
    }

    if (config.hooks)
        hooks_run(config.hooks, HOOK_TRIGGER_IDLE, state);

    g_message("Idle: %s", BOOLSTR(state));
}

static void
lock_callback(LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state == logind_get_locked_hint(c))
        return;

    logind_set_locked_hint(c, state);

    if (state) {
        systemd_start_unit(systemd_ctx, "graphical-lock.target");
    } else {
        set_idle(FALSE);
        systemd_start_unit(systemd_ctx, "graphical-unlock.target");
    }

#ifdef DPMS
    set_dpms_locked(state);
#endif /* DPMS */

#ifdef WIREPLUMBER
    mute_audio_locked(state);
#endif /* WIREPLUMBER */

    if (config.hooks)
        hooks_run(config.hooks, HOOK_TRIGGER_LOCK, state);

    g_message("Locked: %s", BOOLSTR(state));
}

static void
sleep_callback(LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state) {
        g_message("Preparing for sleep...");
        systemd_start_unit(systemd_ctx, "user-sleep.target");
        if (config.on_sleep && !logind_get_locked_hint(c))
            logind_lock_session(c, TRUE);
    } else {
        set_idle(FALSE);
        systemd_start_unit(systemd_ctx, "user-sleep-finished.target");
    }

    if (config.hooks)
        hooks_run(config.hooks, HOOK_TRIGGER_SLEEP, state);
}

static void
shutdown_callback(UNUSED LogindContext *c, gboolean state, UNUSED gpointer data)
{
    if (state) {
        g_message("Preparing for shutdown...");
        systemd_start_unit(systemd_ctx, "user-shutdown.target");
    }

    if (config.hooks)
        hooks_run(config.hooks, HOOK_TRIGGER_SHUTDOWN, state);
}

static void
inhibit_callback(DBusServer *s, const gchar *who, const gchar *why,
        UNUSED guint n, UNUSED gpointer data)
{
    g_debug("Inhibitor added: who='%s' why='%s'", who, why);

    inhibited = TRUE;
    dbus_session_set_inhibited_hint(s->session, TRUE);
    inactive = FALSE;
    timeline_stop(&timeline);

#ifdef DPMS
    DPMSDisable(xsource->dpy);
#endif /* DPMS */
}

static void
uninhibit_callback(DBusServer *s, const gchar *who, const gchar *why,
        guint n, UNUSED gpointer data)
{
    g_debug("Inhibitor removed: who='%s' why='%s'", who, why);

    if (n > 0)
        return;
    inhibited = FALSE;
    dbus_session_set_inhibited_hint(s->session, FALSE);
    timeline_start(&timeline);

#ifdef DPMS
    DPMSEnable(xsource->dpy);
#endif /* DPMS */
}

static gboolean
backlights_cb(BacklightAction a, const char *path, struct Backlight *bl)
{
    struct BacklightConf *c = NULL;

    switch (a) {
        case BL_ACTION_ADD:
            dbus_server_add_backlight(server, bl);
            if (config.backlights)
                c = g_hash_table_lookup(config.backlights, path);
            if (c && c->dim_sec)
                timeline_add_timeout(&timeline, c->dim_sec);
            break;
        case BL_ACTION_REMOVE:
            dbus_server_remove_backlight(server, path);
            if (config.backlights)
                c = g_hash_table_lookup(config.backlights, path);
            if (c && c->dim_sec)
                timeline_remove_timeout(&timeline, c->dim_sec);
            break;
        case BL_ACTION_CHANGE:
        case BL_ACTION_ONLINE:
        case BL_ACTION_OFFLINE:
            dbus_server_update_backlight(server, bl);
            break;
    }

    return TRUE;
}

static void
appear_callback(UNUSED LogindContext *c, UNUSED gpointer data)
{
    if (!server) {
        g_debug("* Init DBus server...");
        server = dbus_server_new(logind_ctx);
        g_signal_connect(server, "inhibit", G_CALLBACK(inhibit_callback),
                NULL);
        g_signal_connect(server, "uninhibit", G_CALLBACK(uninhibit_callback),
                NULL);

        g_debug("* Init Backlights source...");
        backlights = backlights_new(main_ctx, backlights_cb);

        server->bl_devices = backlights->devices;

#ifdef WIREPLUMBER
        g_debug("* Init WirePlumber connection...");
        server->wp_conn = wireplumber_connect(main_ctx, wireplumber_cb);
#endif /* WIREPLUMBER */
    }

    logind_set_idle_hint(logind_ctx, FALSE);
    logind_set_locked_hint(logind_ctx, FALSE);
}

static void
vanish_callback(UNUSED LogindContext *c, UNUSED gpointer data)
{
    dbus_server_free(server);
    server = NULL;
}

static gboolean
xsource_cb(UNUSED gpointer user_data)
{
    if (!xsource->connected) {
        g_warning("X connection lost");
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
on_timeout(guint timeout, gboolean state, gconstpointer user_data)
{
    if (timeout == config.idle_sec)
        set_idle(state);

    if (config.backlights)
        backlights_on_timeout(backlights->devices, config.backlights, timeout,
                state, logind_ctx);

    if (config.hooks)
        hooks_on_timeout(config.hooks, timeout, state);

    if (state)
        dbus_server_emit_inactive(server, timeout);
    else if (inactive)
        dbus_server_emit_active(server);

    inactive = state;
}

static gboolean
load_config(Config *c)
{
    const gchar *dir = g_get_user_config_dir();

    gchar *path = dir ? g_strjoin("/", dir, "sessiond", NULL) :
        g_strjoin("/", getenv("HOME"), ".config", "sessiond", NULL);

    gchar *config = config_path ? config_path :
        g_strjoin("/", path, "sessiond.conf", NULL);

    gchar *hooksd = hooksd_path ? hooksd_path :
        g_strjoin("/", path, "hooks.d", NULL);

    gboolean ret = FALSE;

    if (!g_file_test(config, G_FILE_TEST_EXISTS)) {
        g_warning("No config file found at %s", config);
        goto err;
    }

    ret = config_load(config, hooksd, c);

err:
    if (!config_path)
        g_free(config);
    if (!hooksd_path)
        g_free(hooksd);
    g_free(path);

    return ret;
}

static void
init_timeline(Timeline *tl)
{
    *tl = timeline_new(main_ctx, on_timeout, NULL);

    timeline_add_timeout(tl, config.idle_sec);

    if (config.hooks)
        hooks_add_timeouts(config.hooks, tl);

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
    if (!logind_ctx) {
        logind_ctx = logind_context_new();
        g_signal_connect(logind_ctx, "lock", G_CALLBACK(lock_callback), NULL);
        g_signal_connect(logind_ctx, "sleep", G_CALLBACK(sleep_callback), NULL);
        g_signal_connect(logind_ctx, "shutdown", G_CALLBACK(shutdown_callback), NULL);
        g_signal_connect(logind_ctx, "appear", G_CALLBACK(appear_callback), NULL);
        g_signal_connect(logind_ctx, "vanish", G_CALLBACK(vanish_callback), NULL);
    }

    if (!systemd_ctx)
        systemd_ctx = systemd_context_new();
}

static gboolean
reload_signal(UNUSED gpointer user_data)
{
    Config c;

    if (!load_config(&c)) {
        config_free(&c);
        g_message("Keeping current configuration");
        return TRUE;
    }

    g_message("Reloading configuration files...");

    XSource *s;
    if (c.input_mask != config.input_mask && init_xsource(&s)) {
        xsource_free(xsource);
        xsource = s;
    }

    config_free(&config);
    config = c;
    timeline_free(&timeline);
    init_timeline(&timeline);
    init_dbus();
#ifdef DPMS
    if (!no_dpms)
        set_dpms(&config);
#endif /* DPMS */

    return TRUE;
}

static void
cleanup(void)
{
    g_free(config_path);
    g_free(hooksd_path);
    if (backlights) {
        backlights_restore(backlights->devices, logind_ctx);
        backlights_free(backlights);
    }
    config_free(&config);
    timeline_free(&timeline);
    logind_context_free(logind_ctx);
    systemd_context_free(systemd_ctx);

#ifdef WIREPLUMBER
    wireplumber_disconnect(server->wp_conn);
#endif /* WIREPLUMBER */

    dbus_server_free(server);
    xsource_free(xsource);
    if (main_ctx)
        g_main_context_unref(main_ctx);
    if (main_loop)
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
        {"hooksd", 'd', 0, G_OPTION_ARG_FILENAME, &hooksd_path,
            "Path to hooks directory", "DIR"},
        {"idle-sec", 'i', 0, G_OPTION_ARG_INT, &idle_sec,
            "Seconds the session must be inactive before considered idle",
            "SEC"},
        {"version", 'v', 0, G_OPTION_ARG_NONE, &version, "Show version", NULL},
        {NULL},
    };

    GOptionContext *ctx = g_option_context_new("- standalone X session manager");
    g_option_context_add_main_entries(ctx, opts, NULL);

    GError *err = NULL;
    g_option_context_parse(ctx, &argc, &argv, &err);
    g_option_context_free(ctx);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return EXIT_FAILURE;
    }

    if (version) {
        printf("%s\n", VERSION);
        return EXIT_SUCCESS;
    }

    atexit(cleanup);

    g_debug("* Loading configuration...");
    config = config_new();
    load_config(&config);
    if (idle_sec)
        config.idle_sec = idle_sec;

    main_loop = g_main_loop_new(NULL, FALSE);
    main_ctx = g_main_loop_get_context(main_loop);

    g_debug("* Init X source...");
    if (!init_xsource(&xsource))
        return EXIT_FAILURE;

    g_debug("* Init DBus connections...");
    init_dbus();

    g_debug("* Init timeline...");
    init_timeline(&timeline);

#ifdef DPMS
    set_dpms(&config);
#endif /* DPMS */

    g_unix_signal_add(SIGINT, quit_signal, NULL);
    g_unix_signal_add(SIGTERM, quit_signal, NULL);
    g_unix_signal_add(SIGHUP, reload_signal, NULL);

    g_debug("* Running main loop...");
    g_main_loop_run(main_loop);
}
