#!/bin/sh

# install setuid helper
install -Dm4755 "$MESON_BUILD_ROOT"/sessiond-sysfs-writer \
        "$DESTDIR/$MESON_INSTALL_PREFIX"/lib/sessiond/sessiond-sysfs-writer
