---
title: sessiond-inhibit.1
---

# NAME

sessiond-inhibit - manage sessiond inhibitors

# SYNOPSIS

**sessiond-inhibit** \[options\] \[COMMAND\]

# DESCRIPTION

sessiond-inhibit creates an inhibitor lock before running _COMMAND_ and
releases it when the command returns.
If no command is provided, it lists running inhibitors.

# OPTIONS

- **-h**

    Show help message.

- **-w** _WHO_

    Set who is inhibiting.

- **-y** _WHY_

    Set why this inhibitor is running.

- **-s**

    Stop running inhibitors.

- **-i**

    Inhibit without a command.

- **-u** \[_ID_\]

    Uninhibit last inhibitor or by ID.

# AUTHOR

James Reed <jcrd@sessiond.org>

# REPORTING BUGS

Bugs and issues can be reported here: [https://github.com/jcrd/sessiond/issues](https://github.com/jcrd/sessiond/issues)

# COPYRIGHT

Copyright 2019-2020 James Reed. sessiond is licensed under the
GNU General Public License v3.0 or later.

# SEE ALSO

[sessiond(1)]({{< ref "/man/sessiond.1.md" >}}), [systemd-inhibit(1)](https://www.commandlinux.com/man-page/man1/systemd-inhibit.1.html)
