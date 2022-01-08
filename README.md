# sessiond

[![test][test-badge]][test]
[![CodeQL][codeql-badge]][codeql]

[test-badge]: https://github.com/jcrd/sessiond/actions/workflows/test.yml/badge.svg
[test]: https://github.com/jcrd/sessiond/actions/workflows/test.yml
[codeql-badge]: https://github.com/jcrd/sessiond/actions/workflows/codeql-analysis.yml/badge.svg
[codeql]: https://github.com/jcrd/sessiond/actions/workflows/codeql-analysis.yml

## Overview

sessiond is a standalone X session manager that reports the idle status of a
graphical session to **systemd-logind**. It can be used alongside a window
manager or desktop environment that does not provide its own session management.

## Features

* automatic screen locking on session idle and before sleeping
* automatic backlight dimming on session idle
* systemd targets activated by systemd-logind's lock, unlock, sleep,
  and shutdown signals
* hooks triggered by inactivity or signals
* a DBus service
    * backlight interaction
* (optional) management of DPMS settings

## Documentation

Documentation is available at [sessiond.org](https://sessiond.org/).

See the [Getting started](https://sessiond.org/getting-started/) section to get
started using sessiond.

## License

sessiond is licensed under the GNU General Public License v3.0 or later
(see [LICENSE](LICENSE)).
