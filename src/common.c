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

#include "common.h"

#include <stdio.h>
#include <glib-2.0/glib.h>

gint
spawn_exec(gchar **argv)
{
    gint status = -1;
    gchar *std_out = NULL;
    gchar *std_err = NULL;
    GError *err = NULL;

    g_spawn_sync(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL,
                 &std_out, &std_err, &status, &err);

    if (err)
        goto error;

    if (std_out) {
        fprintf(stdout, "%s\n", std_out);
        g_free(std_out);
    }
    if (std_err) {
        fprintf(stderr, "%s\n", std_err);
        g_free(std_err);
    }

    g_spawn_check_wait_status(status, &err);

    if (err) {
    error:
        g_warning("%s", err->message);
        g_error_free(err);
        return status;
    }

    return status;
}
