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

#include "config.h"
#include "backlight.h"
#include "hooks.h"
#include "xsource.h"

#include <stdio.h>
#include <glib-2.0/glib.h>
#include "toml/toml.h"

static gchar *
subsystem_from_path(const gchar *path)
{
    GRegex *regex = g_regex_new("/sys/class/(.*)/.*", 0, 0, NULL);

    GMatchInfo *match;
    g_regex_match(regex, path, 0, &match);
    gchar *sys = g_match_info_fetch(match, 1);

    g_match_info_free(match);
    g_regex_unref(regex);

    return sys;
}

static struct BacklightConf *
new_backlight(const gchar *path)
{
    struct BacklightConf *bl = g_malloc(sizeof(struct BacklightConf));

    gchar *subsystem = subsystem_from_path(path);

    if (g_strcmp0(subsystem, "backlight") == 0) {
        bl->dim_sec = 60 * 8;
        bl->dim_value = -1;
        bl->dim_percent = 0.3;
    } else if (g_strcmp0(subsystem, "leds") == 0) {
        bl->dim_sec = 60;
        bl->dim_value = 0;
        bl->dim_percent = -1;
    } else {
        g_warning("Unrecognized backlight path: %s", path);
        g_free(bl);
        return NULL;
    }

    g_free(subsystem);
    return bl;
}

static toml_table_t *
parse_file(const gchar *path)
{
    FILE *fp = fopen(path, "r");

    if (!fp) {
        perror("fopen");
        return NULL;
    }

    char errbuf[BUFSIZ];
    toml_table_t *tab = toml_parse_file(fp, errbuf, sizeof(errbuf));

    if (!tab)
        g_warning("Failed to parse configuration %s: %s", path, errbuf);

    fclose(fp);

    return tab;
}

static gint
load_int(toml_table_t *tab, const char *key, gint *ret)
{
    const char *raw = toml_raw_in(tab, key);

    if (!raw)
        return 0;

    int64_t i;

    if (toml_rtoi(raw, &i) == -1) {
        g_warning("Failed to parse %s: expected integer", key);
        return -1;
    }

    *ret = (gint)i;
    return 0;
}

static gint
load_uint(toml_table_t *tab, const char *key, guint *ret)
{
    gint i = -1;
    gint r = load_int(tab, key, &i);
    if (i > -1)
        *ret = (guint)i;
    return r;
}

static gdouble
load_double(toml_table_t *tab, const char *key, gdouble *ret)
{
    const char *raw = toml_raw_in(tab, key);

    if (!raw)
        return 0;

    double d;

    if (toml_rtod(raw, &d) == -1) {
        g_warning("Failed to parse %s: expected double", key);
        return -1;
    }

    *ret = (gdouble)d;
    return 0;
}

static gint
load_bool(toml_table_t *tab, const char *key, gboolean *ret)
{
    const char *raw = toml_raw_in(tab, key);

    if (!raw)
        return 0;

    int b;

    if (toml_rtob(raw, &b) == -1) {
        g_warning("Failed to parse %s: expected boolean", key);
        return -1;
    }

    *ret = (gboolean)b;
    return 0;
}

static gint
load_str(toml_table_t *tab, const char *key, gchar **ret)
{
    const char *raw = toml_raw_in(tab, key);

    if (!raw)
        return 0;

    char *str;

    if (toml_rtos(raw, &str) == -1) {
        g_warning("Failed to parse %s: expected string", key);
        return -1;
    }

    *ret = g_strdup(str);
    return 0;
}

static gint
load_exec(toml_table_t *tab, const char *key, gchar ***ret)
{
    gchar *str = NULL;
    load_str(tab, key, &str);

    if (!str)
        return 0;

    gchar **argv;
    GError *err = NULL;
    g_shell_parse_argv(str, NULL, &argv, &err);

    if (err) {
        g_warning("Failed to parse %s: %s", key, err->message);
        g_error_free(err);
        g_free(str);
        return -1;
    }

    g_free(str);
    *ret = argv;
    return 0;
}

static gint
load_input_mask(toml_table_t *tab, const char *key, guint *ret)
{
    int len;
    toml_array_t *inputs = toml_array_in(tab, key);

    if (!inputs || (len = toml_array_nelem(inputs)) == 0)
        return 0;

    if (toml_array_kind(inputs) != 'v') {
        g_warning("Failed to parse %s: expected array of values", key);
        return -1;
    }

    if (toml_array_type(inputs) != 's') {
        g_warning("Failed to parse %s: expected array of strings", key);
        return -1;
    }

    guint mask = 0;

    for (int i = 0; i < toml_array_nelem(inputs); i++) {
        const char *raw = toml_raw_at(inputs, i);
        char *str;
        if (toml_rtos(raw, &str) == -1) {
            g_warning("Failed to parse %s at index %d", key, i);
            return -1;
        }
#define X(t, n) \
        if (g_strcmp0(str, n) == 0) { \
            mask |= INPUT_TYPE_MASK(t); \
            goto end; \
        }
        INPUT_TYPE_LIST
#undef X
end:
        free(str);
    }

    *ret = mask;
    return 0;
}

static gint
load_backlights(toml_table_t *tab, const char *key, GHashTable *out)
{
    int len;
    toml_array_t *backlights = toml_array_in(tab, key);

    if (!backlights || (len = toml_array_nelem(backlights)) == 0)
        return 0;

    if (toml_array_kind(backlights) != 't') {
        g_warning("Failed to parse %s: expected array of tables", key);
        return -1;
    }

    for (int i = 0; i < len; i++) {
        toml_table_t *t = toml_table_at(backlights, i);

        gchar *path = NULL;
        load_str(t, "Path", &path);

        if (!path)
            continue;

        struct BacklightConf *bl = new_backlight(path);

        if (!bl)
            continue;

#define X(key, type, name) \
        load_##type(t, key, &bl->name);
        BACKLIGHT_TABLE_LIST
#undef X

        if (bl->dim_percent != -1)
            bl->dim_percent = CLAMP(bl->dim_percent, 0.01, 1.0);

        g_hash_table_insert(out, path, bl);
    }

    return 0;
}

static gint
load_trigger(toml_table_t *tab, const char *key, guint *ret)
{
    gchar *str = NULL;
    load_str(tab, key, &str);

    if (!str)
        return 0;

#define X(t, n) \
    if (g_strcmp0(str, n) == 0) { \
        *ret = HOOK_TRIGGER_##t; \
        return 0; \
    }
    HOOK_TRIGGER_LIST
#undef X

    g_warning("Failed to parse %s: unknown trigger %s", key, str);
    return -1;
}

static void
free_hook(struct Hook *hook)
{
    if (!hook)
        return;
    g_strfreev(hook->exec_start);
    g_strfreev(hook->exec_stop);
    g_free(hook);
}

static gint
load_hook(toml_table_t *tab, GPtrArray *out, const gchar **err)
{
    struct Hook *h = g_malloc0(sizeof(struct Hook));

#define X(key, type, name) \
    load_##type(tab, key, &h->name);
    HOOKS_TABLE_LIST
#undef X

    if (!h->trigger) {
        *err = "expected Trigger key";
        goto err;
    } else if (h->trigger == HOOK_TRIGGER_INACTIVE) {
        if (!h->inactive_sec) {
            *err = "expected InactiveSec key";
            goto err;
        }
    } else if (!(h->exec_start || h->exec_stop)) {
        *err = "expected ExecStart key or ExecStop key";
        goto err;
    }

    g_ptr_array_add(out, h);
    return 0;

err:
    free_hook(h);
    return -1;
}

static gint
load_hooks(toml_table_t *tab, const char *key, GPtrArray *out)
{
    int len;
    toml_array_t *hooks = toml_array_in(tab, key);

    if (!hooks || (len = toml_array_nelem(hooks)) == 0)
        return 0;

    if (toml_array_kind(hooks) != 't') {
        g_warning("Failed to parse %s: expected array of tables", key);
        return -1;
    }

    for (int i = 0; i < len; i++) {
        const gchar *err;
        if (load_hook(toml_table_at(hooks, i), out, &err) == -1) {
            g_warning("Failed to parse %s at index %d: %s", key, i, err);
            return -1;
        }
    }

    return 0;
}

static gint
load_hooks_dir(const gchar *path, GPtrArray *out)
{
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
        g_debug("Load hooks: directory not found at %s; skipping", path);
        return 0;
    }

    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);

    if (err) {
        g_warning("Failed to load hook files: %s", err->message);
        g_error_free(err);
        return -1;
    }

    gint ret = 0;
    const gchar *name;

    while ((name = g_dir_read_name(dir))) {
        if (!g_str_has_suffix(name, ".hook"))
            continue;

        gchar *p = g_strjoin("/", path, name, NULL);
        toml_table_t *hook = parse_file(p);

        toml_table_t *tab = toml_table_in(hook, "Hook");

        if (!tab) {
            g_warning("Failed to parse hook %s: expected Hook table", p);
            ret = -1;
            goto end;
        }

        const gchar *err;
        if (load_hook(tab, out, &err) == -1) {
            g_warning("Failed to parse hook %s: %s", p, err);
            ret = -1;
        }

end:
        g_free(p);
    }

    g_dir_close(dir);
    return ret;
}

Config
config_new(void)
{
    Config c;

    c.input_mask = INPUT_TYPE_MASK(RawMotion)
        | INPUT_TYPE_MASK(RawButtonPress) | INPUT_TYPE_MASK(RawKeyPress);
    c.idle_sec = 60 * 20;
    c.on_idle = TRUE;
    c.on_sleep = TRUE;
    c.backlights = NULL;
    c.hooks = NULL;
#ifdef DPMS
    c.dpms_enable = TRUE;
    c.standby_sec = 60 * 10;
    c.suspend_sec = 60 * 10;
    c.off_sec = 60 * 10;
#endif /* DPMS */

    return c;
}

gboolean
config_load(const gchar *path, const gchar *hooksd, Config *c)
{
    toml_table_t *conf = parse_file(path);

    if (!conf)
        return FALSE;

    gint ret = 0;
    toml_table_t *tab;

#define X(key, type, name) \
    ret += load_##type(tab, key, &c->name);

    if ((tab = toml_table_in(conf, "Idle"))) {
        IDLE_TABLE_LIST
    }

    if ((tab = toml_table_in(conf, "Lock"))) {
        LOCK_TABLE_LIST
    }

#ifdef DPMS
    if ((tab = toml_table_in(conf, "DPMS"))) {
        DPMS_TABLE_LIST
    }
#endif /* DPMS */

#undef X

    c->backlights = g_hash_table_new_full(g_str_hash, g_str_equal,
            (GDestroyNotify)g_free, (GDestroyNotify)g_free);

    ret += load_backlights(conf, "Backlight", c->backlights);

    if (!g_hash_table_size(c->backlights)) {
        g_hash_table_unref(c->backlights);
        c->backlights = NULL;
    }

    c->hooks = g_ptr_array_new_with_free_func((GDestroyNotify)free_hook);

    ret += load_hooks(conf, "Hook", c->hooks);

    if (hooksd)
        ret += load_hooks_dir(hooksd, c->hooks);

    if (!c->hooks->len) {
        g_ptr_array_unref(c->hooks);
        c->hooks = NULL;
    }

    toml_free(conf);

    return ret == 0;
}

void
config_free(Config *c)
{
    if (!c)
        return;

    if (c->backlights)
        g_hash_table_unref(c->backlights);
    if (c->hooks)
        g_ptr_array_free(c->hooks, TRUE);
}
