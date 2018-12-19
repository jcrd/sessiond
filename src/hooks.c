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

#include "hooks.h"
#include "common.h"
#include "keyfile.h"
#include "timeline.h"

#include <glib-2.0/glib.h>

static gboolean
kf_load_exec(GKeyFile *kf, const gchar *g, const gchar *k, gchar ***opt)
{
    gchar *str = NULL;
    if (!kf_load_str(kf, g, k, &str))
        return FALSE;

    GError *err = NULL;
    gchar **argv;
    g_shell_parse_argv(str, NULL, &argv, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        g_free(str);
        return FALSE;
    }

    g_free(str);
    *opt = argv;

    return TRUE;
}

static HookType
get_hook_type(const gchar *name)
{
#define X(t, n) \
    if (g_strcmp0(name, n) == 0) \
        return HOOK_TYPE_##t;
    HOOK_TYPE_LIST
#undef X

    return HOOK_TYPE_NONE;
}

static void
free_hook(Hook *hook)
{
    if (!hook)
        return;
    g_strfreev(hook->exec_start);
    g_strfreev(hook->exec_stop);
    g_free(hook);
}

static Hook *
load_hook(const gchar *path)
{
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return NULL;
    }

    Hook *hook = NULL;
    gchar *str = NULL;

    if (!kf_load_str(kf, "Hook", "Trigger", &str)) {
    error:
        g_key_file_free(kf);
        free_hook(hook);
        return NULL;
    }

    if (!str) {
        g_warning("'Trigger' key is missing in %s", path);
        goto error;
    }

    hook = g_malloc0(sizeof(Hook));
    hook->type = get_hook_type(str);

    if (hook->type == HOOK_TYPE_NONE) {
        g_warning("Invalid hook type: %s", str);
        g_free(str);
        goto error;
    }

    g_free(str);

    hook->exec_start = NULL;
    if (!kf_load_exec(kf, "Hook", "ExecStart", &hook->exec_start))
        goto error;

    hook->exec_stop = NULL;
    if (!kf_load_exec(kf, "Hook", "ExecStop", &hook->exec_stop))
        goto error;

    if (hook->type == HOOK_TYPE_INACTIVE) {
        gint sec = -1;
        if (!kf_load_int(kf, "Hook", "InactiveSec", &sec))
            goto error;
        if (sec < 0) {
            g_warning("'InactiveSec' key is missing or invalid in %s", path);
            goto error;
        }
        hook->inactive_sec = sec;
    }

    g_key_file_free(kf);

    return hook;
}

static void
run_hooks_timeout(GPtrArray *hooks, HookType type, gboolean state,
                  guint timeout)
{
    for (guint i = 0; i < hooks->len; i++) {
        Hook *h = g_ptr_array_index(hooks, i);
        if (h->type != type
            || (type == HOOK_TYPE_INACTIVE && h->inactive_sec != timeout))
            continue;
        if (state && h->exec_start)
            spawn_exec(h->exec_start);
        else if (!state && h->exec_stop)
            spawn_exec(h->exec_stop);
    }
}

GPtrArray *
hooks_load(const gchar *path)
{
    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return NULL;
    }

    GPtrArray *hs = NULL;
    const gchar *name;

    while ((name = g_dir_read_name(dir))) {
        if (!g_str_has_suffix(name, ".hook"))
            continue;

        gchar *p = g_strjoin("/", path, name, NULL);
        Hook *hook = load_hook(p);

        if (hook) {
            if (!hs)
                hs = g_ptr_array_new_with_free_func((GDestroyNotify)free_hook);
            g_ptr_array_add(hs, hook);
        }

        g_free(p);
    }

    g_dir_close(dir);

    return hs;
}

void
hooks_add_timeouts(GPtrArray *hooks, Timeline *tl)
{
    for (guint i = 0; i < hooks->len; i++) {
        Hook *h = g_ptr_array_index(hooks, i);
        if (h->type == HOOK_TYPE_INACTIVE)
            timeline_add_timeout(tl, h->inactive_sec);
    }
}

void
hooks_run(GPtrArray *hooks, HookType type, gboolean state)
{
    if (type == HOOK_TYPE_INACTIVE)
        return;
    run_hooks_timeout(hooks, type, state, 0);
}

void
hooks_on_timeout(GPtrArray *hooks, guint timeout, gboolean state)
{
    run_hooks_timeout(hooks, HOOK_TYPE_INACTIVE, state, timeout);
}

void
hooks_free(GPtrArray *hooks)
{
    if (hooks)
        g_ptr_array_free(hooks, TRUE);
}
