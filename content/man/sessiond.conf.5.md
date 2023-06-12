---
title: sessiond.conf.5
---

# NAME

sessiond.conf - sessiond configuration file format

# SYNOPSIS

_XDG\_CONFIG\_HOME_/sessiond/sessiond.conf or
_HOME_/.config/sessiond/sessiond.conf

# DESCRIPTION

This file configures the X session manager [sessiond(1)]({{< ref "/man/sessiond.1.md" >}}).
Its syntax is toml v0.5.0.
See: https://github.com/toml-lang/toml/tree/v0.5.0.

# OPTIONS

## \[Idle\]

- _Inputs=_

    A list (of the format \["item", "item"\]) of input event types used to determine
    activity.  Values are "motion", "button-press", "button-release", "key-press",
    "key-release".

- _IdleSec=_

    Seconds the session must be inactive before considered idle.

## \[Lock\]

- _OnIdle=_

    If "true", lock the session when it becomes idle.

- _OnSleep=_

    If "true", lock the session when [systemd-logind(8)](https://www.commandlinux.com/man-page/man8/systemd-logind.8.html) sends the
    "PrepareForSleep" signal.

- _StandbySec=_

    DPMS standby timeout in seconds to use while session is locked.
    Must occur before or simultaneously with Suspend timeout.

- _SuspendSec=_

    DPMS suspend timeout in seconds to use while session is locked.
    Must occur before or simultaneously with Off timeout.

- _OffSec=_

    DPMS off timeout in seconds to use while session is locked.

- _MuteAudio=_

    If "true", mute the default audio sink while the session is locked.
    The mute state will be restored when unlocked.

## \[DPMS\]

- _Enable=_

    If "true", apply DPMS settings, including those in the "\[Lock\]" section.

- _StandbySec=_

    DPMS standby timeout in seconds. Must occur before or simultaneously with
    Suspend timeout.

- _SuspendSec=_

    DPMS suspend timeout in seconds. Must occur before or simultaneously with
    Off timeout.

- _OffSec=_

    DPMS off timeout in seconds.

## \[\[Backlight\]\]

Backlights are configured as an array of tables, using the section
"\[\[Backlight\]\]". The options will be applied to backlights with the same path.

- _Path=_

    Path to the backlight device via sys mount point. Should be of the format:
    "/sys/class/_subsystem_/_name_".

- _DimSec=_

    Seconds the session must be inactive before the backlight is dimmed.

- _DimValue=_

    Value of the backlight brightness when dimming.

- _DimPercent=_

    Percentage to lower backlight brightness when dimming.

## \[\[Hook\]\]

Hooks are configured as an array of tables, using the section "\[\[Hook\]\]".
See [sessiond-hooks(5)]({{< ref "/man/sessiond-hooks.5.md" >}}) for a description of options.

# SEE ALSO

[sessiond(1)]({{< ref "/man/sessiond.1.md" >}}), [systemd-logind.service(8)](https://www.commandlinux.com/man-page/man8/systemd-logind.service.8.html), [sessiond-hooks(5)]({{< ref "/man/sessiond-hooks.5.md" >}})
