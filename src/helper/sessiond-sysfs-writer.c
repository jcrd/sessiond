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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

int
main(int argc, char *argv[])
{
    char path[MAXPATHLEN];
    FILE *f;

    if (argc != 3) {
        fprintf(stderr, "Expected 2 arguments: PATH VALUE\n");
        return EXIT_FAILURE;
    }

    if (strncmp("/sys", argv[1], 4) == 0) {
        f = fopen(argv[1], "w");
    } else {
        snprintf(path, sizeof(path), "/sys%s", argv[1]);
        f = fopen(path, "w");
    }

    if (!f) {
        perror("Failed to open path");
        return EXIT_FAILURE;
    }

    if (fprintf(f, "%s", argv[2]) < 0) {
        fclose(f);
        fprintf(stderr, "Failed to write to path\n");
        return EXIT_FAILURE;
    }

    fclose(f);
}
