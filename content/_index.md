---
title: Overview
---

sessiond is a daemon for **systemd**-based Linux systems that interfaces with
**systemd-logind** to provide session management features to X11 window managers.

Its primary responsibility is to monitor keyboard and mouse activity to
determine when a session has become idle, and to then act accordingly.

It also provides a DBus service with interfaces to backlights and audio sinks.

## Features

* automatic screen locking on session idle and before sleeping
* automatic backlight dimming on session idle
* automatic muting of audio while session is locked
* systemd targets activated by systemd-logind's lock, unlock, sleep,
  and shutdown signals
* hooks triggered by inactivity or signals
* a DBus service
    * backlight interaction
    * audio sink interaction
* (optional) management of DPMS settings

{{< button size="large" relref="/getting-started" >}}Get started{{< /button >}}
