---
title: sessionctl.1
---

# NAME

sessionctl - standalone X session manager client

# SYNOPSIS

**sessionctl** \[command\]

# DESCRIPTION

sessionctl is responsible for running a sessiond session and interacting with
its DBus service.

# COMMANDS

- **run** \[_SERVICE_\]

    Run a new session, with _SERVICE_ as the window manager service if provided.
    By default, the service installed under the _window-manager.service_ alias is
    used.

- **stop**

    Stop the running session.

- **status**

    Show session status.

- **lock**

    Lock the session.

- **unlock**

    Unlock the session.

- **properties**

    List sessiond properties.

- **backlight** \[_NAME_\]

    Interact with backlights.
    If backlight _NAME_ is given with no options, print brightness.
    If no arguments are given, list backlights.

    Options:

    - **-s** _VALUE_, **--set** _VALUE_

        Set backlight brightness.

    - **-i** _VALUE_, **--inc** _VALUE_

        Increment backlight brightness.
        Prints the new brightness value.

- **audiosink** \[_ID_\]

    Interact with audio sinks.
    If audio sink _ID_ is given with no options, print volume and mute state.
    If no arguments are given, list audio sinks.

    Options:

    - **-s** _VALUE_, **--set** _VALUE_

        Set audio sink volume.

    - **-i** _VALUE_, **--inc** _VALUE_

        Increment audio sink volume.
        Prints the new volume value.

    - **-m**, **--mute**

        Mute audio sink.

    - **-u**, **--unmute**

        Unmute audio sink.

    - **-t**, **--toggle-mute**

        Toggle audio sink mute state.
        Prints the new mute state.

- **version**

    Show sessiond version.

# AUTHOR

James Reed <jcrd@sessiond.org>

# REPORTING BUGS

Bugs and issues can be reported here: [https://github.com/jcrd/sessiond/issues](https://github.com/jcrd/sessiond/issues)

# COPYRIGHT

Copyright 2019-2020 James Reed. sessiond is licensed under the
GNU General Public License v3.0 or later.

# SEE ALSO

[systemctl(1)](https://www.commandlinux.com/man-page/man1/systemctl.1.html), [gdbus(1)](https://www.commandlinux.com/man-page/man1/gdbus.1.html)
