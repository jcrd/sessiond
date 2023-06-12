---
title: sessiond.1
---

# NAME

sessiond - standalone X session manager

# SYNOPSIS

**sessiond** \[OPTIONS\]

# DESCRIPTION

sessiond is a standalone X session manager that reports the idle status of a
session to [systemd-logind.service(8)](https://www.commandlinux.com/man-page/man8/systemd-logind.service.8.html) and handles its lock, unlock, sleep, and
shutdown signals. sessiond also provides hooks triggered by inactivity or a
signal, automatic backlight dimming on idle, and optional management of DPMS
settings.

# OPTIONS

- **-h**, **--help**

    Show help options.

- **-c**, **--config**=_CONFIG_

    Path to config file. See [sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}}) for configuration options.

- **-i**, **--idle-sec**=_SEC_

    Seconds the session must be inactive before considered idle.

- **-v**, **--version**

    Show version.

# DEBUGGING

Running sessiond with the environment variable _G\_MESSAGES\_DEBUG_ set to "all"
will print debug messages.

# AUTHOR

James Reed <jcrd@sessiond.org>

# REPORTING BUGS

Bugs and issues can be reported here: [https://github.com/jcrd/sessiond/issues](https://github.com/jcrd/sessiond/issues)

# COPYRIGHT

Copyright 2018-2020 James Reed. sessiond is licensed under the
GNU General Public License v3.0 or later.

# SEE ALSO

[sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}}), [sessiond-hooks(5)]({{< ref "/man/sessiond-hooks.5.md" >}}), [sessiond-dbus(8)]({{< ref "/man/sessiond-dbus.8.md" >}}), [systemd-logind.service(8)](https://www.commandlinux.com/man-page/man8/systemd-logind.service.8.html)
