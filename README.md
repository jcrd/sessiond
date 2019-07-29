# sessiond [![CircleCI](https://circleci.com/gh/jcrd/sessiond.svg?style=svg)](https://circleci.com/gh/jcrd/sessiond)

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
* (optional) management of DPMS settings

## Building

### Dependencies

* bash [*runtime*]
* perl [*build*,*runtime*]
* grep [*runtime*]
* coreutils [*runtime*]
* meson >= 0.47.0 [*build*]
* glib >= 2.52 [*build*,*runtime*]
* libx11 [*build*]
* libxi [*build*]
* libxext (optional, for DPMS support) [*build*]

Ensure the above build dependencies are satisfied and configure the build with
**meson**:

```
meson builddir && cd builddir
```

Inside the build directory, `meson configure` will list all options set with
`-D`. For example, to disable the DPMS feature, run:

```
meson configure -Ddpms=disabled
```

Finally, run `ninja` to build sessiond.

### Testing

Run tests with `meson test`.

### Installing

After building sessiond, `ninja install` can be used to install it.

## Configuration

sessiond looks for a configuration file at
`$XDG_CONFIG_HOME/sessiond/sessiond.conf`
or falls back to `$HOME/.config/sessiond/sessiond.conf` if `$XDG_CONFIG_HOME`
is unset.

The default configuration is included at `/usr/share/sessiond/sessiond.conf`.

See [sessiond.conf(5)](man/sessiond.conf.5.pod) for descriptions of the options.

## systemd targets

sessiond provides the following systemd targets:

Target                    | Started when
------                    | ------------
`graphical-lock.target`   | session is locked
`graphical-unlock.target` | session is unlocked
`graphical-idle.target`   | session becomes idle
`graphical-unidle.target` | session resumes activity
`user-sleep.target`       | system sleeps
`user-shutdown.target`    | system shuts down

## Hooks

Hook files with the `.hook` suffix are read from
`$XDG_CONFIG_HOME/sessiond/hooks.d` or `$HOME/.config/sessiond/hooks.d`.

Hooks provide functionality similar to systemd targets but can also be triggered
by a period of inactivity.

See [sessiond-hooks(5)](man/sessiond-hooks.5.pod) for more information.

## DBus service

sessiond provides a DBus service on the session bus at the well-known name
**_org.sessiond.session1_**.

See the _DBus Service_ section of [sessiond(1)](man/sessiond.1.pod) for
descriptions of methods, properties, and signals.

For complete introspection data, use **gdbus**:

```
gdbus introspect --session --dest org.sessiond.session1 --object-path /org/sessiond/session1
```

## sessionctl

The `sessionctl` script is provided to run a sessiond session and interact with
its DBus service.

```
usage: sessionctl [command]

With no command, show session status.

commands:
  run         Run session
  stop        Stop the running session
  status      Show session status
  lock        Lock the session
  unlock      Unlock the session
  properties  List session properties
  backlight   Interact with backlights
                Subcommands: list, get, set
  version     Show sessiond version
```

See [sessionctl(1)](man/sessionctl.1.pod) for more information.

## Managing the session

### Starting the session

A sessiond-based session should be started via a display manager, using
the provided `sessiond.desktop` X session file.

For example, configure `lightdm` to start a sessiond session by setting
`user-session=sessiond` in `/etc/lightdm/lightdm.conf`.

### Running services

`sessiond-session.target` binds to `graphical-session.target` provided
by systemd. See the _Special Passive User Units_ section of
[systemd.special(7)][2] for more information about graphical sessions.

To run a service in the graphical session, `<service>.service` should contain:

```
[Unit]
PartOf=graphical-session.target
```

so the service is stopped when the session ends, and:

```
[Install]
WantedBy=graphical-session.target
```

so the service is started when the session begins.

It can then be enabled with `systemctl --user enable <service>`.

See below for example services.

### Locking the session

By default, the session is locked when it becomes idle and before sleeping.
This is configured in the `[Lock]` section of the configuration file.
The session can be manually locked by running `loginctl lock-session`.

To configure a service to act as the screen locker, include:

```
[Service]
ExecStopPost=/usr/bin/loginctl unlock-session
```

or

```
[Service]
ExecStopPost=/usr/bin/sessionctl unlock
```

so the session is considered unlocked when the locker exits, and:

```
[Install]
WantedBy=graphical-lock.target
```

so the service is started when the session locks. Then enable it.

Below is an example `i3lock.service`:

```
[Unit]
Description=Lock X session with i3lock
PartOf=graphical-session.target

[Service]
ExecStart=/usr/bin/i3lock -n -c 000000
ExecStopPost=/usr/bin/loginctl unlock-session

[Install]
WantedBy=graphical-lock.target
```

### Stopping the session

The session can be stopped with `sessionctl stop`. This will stop
`graphical-session.target` and all units that are part of the session.

A service that is part of the graphical session can be responsible for stopping
the session. To configure a service to stop the session when it exits, include:

```
[Service]
ExecStopPost=/usr/bin/sessionctl stop
```

Below is an example `awesome.service` that stops the session when the Awesome
window manager exits:

```
[Unit]
Description=Awesome window manager
PartOf=graphical-session.target

[Service]
ExecStart=/usr/bin/awesome
ExecStopPost=/usr/bin/sessionctl stop

[Install]
WantedBy=graphical-session.target
```
### Inhibiting inactivity

Inhibitor locks can be acquired when the session should be considered active
while a given command is running, e.g. a media player.

The `sessiond-inhibit` script provides a simple interface to acquire a lock
before running a command and release it when the command returns.

```
usage: sessiond-inhibit [options] [COMMAND]

With no COMMAND, list running inhibitors.

options:
  -h      Show help message
  -w WHO  Set who is inhibiting
  -y WHY  Set why this inhibitor is running
```

See [sessiond-inhibit(1)](man/sessiond-inhibit.1.pod) for more information.

See the _DBus Service_ section of [sessiond(1)](man/sessiond.1.pod) for
descriptions of inhibitor-related methods.

## License

sessiond is licensed under the GNU General Public License v3.0 or later
(see [LICENSE](LICENSE)).

[1]: https://mesonbuild.com
[2]: https://www.freedesktop.org/software/systemd/man/systemd.special.html
