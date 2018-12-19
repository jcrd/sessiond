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

#include "backlight.h"
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <glib-2.0/glib.h>

#define SYSFS_WRITER "/usr/lib/sessiond/sessiond-sysfs-writer"
#define SYSFS_BL_DIR "/sys/class/backlight"

static gchar *
get_bl_name(void)
{
    GError *err = NULL;
    GDir *dir = g_dir_open(SYSFS_BL_DIR, 0, &err);

    if (err) {
        g_warning("%s", err->message);
        g_error_free(err);
        return NULL;
    }

    gchar *n = g_strdup(g_dir_read_name(dir));
    g_dir_close(dir);

    return n;
}

gchar *
backlight_get_interface(const gchar *name)
{
    gchar *n = NULL;

    if (!name && !(n = get_bl_name()))
        return NULL;

    gchar *iface = g_strjoin("/", SYSFS_BL_DIR, name ? name : n, "brightness",
                             NULL);

    if (!g_file_test(iface, G_FILE_TEST_EXISTS)) {
        g_warning("%s does not exist", iface);
        g_free(iface);
        g_free(n);
        return NULL;
    }

    g_free(n);
    return iface;
}

gint
backlight_get(const gchar *iface)
{
    FILE *f = fopen(iface, "r");
    unsigned int v;

    if (!f) {
        perror("Failed to open backlight interface");
        return -1;
    }

    if (!fscanf(f, "%u", &v)) {
        fclose(f);
        g_warning("Failed to read from %s", iface);
        return -1;
    }

    fclose(f);
    return v;
}

void
backlight_set(const gchar *iface, guint v)
{
    gchar *s = g_strdup_printf("%u", v);
    gchar *argv[] = {SYSFS_WRITER, (gchar *)iface, s, NULL};

    if (spawn_exec(argv) == 0)
        g_debug("Backlight set to %u", v);

    g_free(s);
}
