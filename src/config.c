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

#include "config.h"
#include "keyfile.h"
#include "xsource.h"

#include <glib-2.0/glib.h>

static gboolean
kf_load_input_mask(GKeyFile *kf, const gchar *g, const gchar *k, guint *opt)
{
    GError *err = NULL;
    gsize len;
    gchar **input = g_key_file_get_string_list(kf, g, k, &len, &err);

    if (err) {
        gboolean ret = FALSE;
        if (err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND ||
            err->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND)
            ret = TRUE;
        else
            g_warning("%s", err->message);
        g_error_free(err);
        return ret;
    }

    for (guint i = 0; i < len; i++) {
        gchar *str = input[i];
#define X(t, n) \
        if (g_strcmp0(str, n) == 0) { \
            *opt |= INPUT_TYPE_MASK(t); \
            continue; \
        }
        INPUT_TYPE_LIST
#undef X
    }

    g_strfreev(input);

    return TRUE;
}

static Config
new_config(void)
{
    Config c;

    c.input_mask = INPUT_TYPE_MASK(RawMotion)
        | INPUT_TYPE_MASK(RawButtonPress) | INPUT_TYPE_MASK(RawKeyPress);
    c.idle_sec = 60 * 20;
    c.on_idle = TRUE;
    c.on_sleep = TRUE;
    c.bl_enable = TRUE;
    c.bl_name = NULL;
    c.dim_sec = 60 * 8;
    c.dim_percent = 30;
#ifdef DPMS
    c.dpms_enable = TRUE;
    c.standby_sec = 60 * 10;
    c.suspend_sec = 60 * 10;
    c.off_sec = 60 * 10;
#endif /* DPMS */

    return c;
}

gboolean
config_load(Config *c, const gchar *path)
{
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_list_separator(kf, ',');

    GError *err = NULL;
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return FALSE;
    }

    *c = new_config();

    gboolean ret = TRUE;

#define X(g, k, type, field) \
    if (!kf_load_##type(kf, g, k, &c->field)) \
        ret = FALSE;
    CONFIG_MAP_LIST
#ifdef DPMS
    DPMS_MAP_LIST
#endif /* DPMS */
#undef X

    g_key_file_free(kf);

    return ret;
}

void
config_free(Config *c)
{
    g_free(c->bl_name);
}
