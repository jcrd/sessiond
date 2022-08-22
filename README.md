# [<img src="https://sessiond.org/favicon/favicon.svg" width="32">][sessiond.org] sessiond

[![test][test-badge]][test]
[![CodeQL][codeql-badge]][codeql]
[![Copr build status][copr-badge]][copr]

[test-badge]: https://github.com/jcrd/sessiond/actions/workflows/test.yml/badge.svg
[test]: https://github.com/jcrd/sessiond/actions/workflows/test.yml
[codeql-badge]: https://github.com/jcrd/sessiond/actions/workflows/codeql-analysis.yml/badge.svg
[codeql]: https://github.com/jcrd/sessiond/actions/workflows/codeql-analysis.yml
[copr-badge]: https://copr.fedorainfracloud.org/coprs/jcrd/sessiond/package/sessiond/status_image/last_build.png
[copr]: https://copr.fedorainfracloud.org/coprs/jcrd/sessiond/package/sessiond/

[sessiond.org]: https://sessiond.org/

## Overview

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

## Documentation

Documentation is available at [sessiond.org][sessiond.org].

See the [Getting started](https://sessiond.org/getting-started/) section to get
started using sessiond.

## License

sessiond is licensed under the GNU General Public License v3.0 or later
(see [LICENSE](LICENSE)).
