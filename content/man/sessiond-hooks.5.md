---
title: sessiond-hooks.5
---

# NAME

sessiond-hooks - sessiond hook file format

# SYNOPSIS

- \[Hook\]
- Trigger=Lock&#x7C;Idle&#x7C;Sleep&#x7C;Shutdown&#x7C;Inactive
- InactiveSec=&lt;seconds> (Inactive only)
- ExecStart=&lt;command>
- ExecStop=&lt;command> (Lock&#x7C;Idle&#x7C;Inactive only)

# DESCRIPTION

sessiond provides the ability to define hooks that are triggered by events.
The "Inactive" event is unique to hooks. It allows commands to be run after a
period of inactivity. It is more general than the "Idle" event, which occurs
after _IdleSec_ (see [sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}})) seconds of inactivity.

Hooks can be specified in the configuration file using the section "\[\[Hook\]\]".
See [sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}}).

Hook files with the ".hook" suffix are read from
_XDG\_CONFIG\_HOME_/sessiond/hooks.d or _HOME_/.config/sessiond/hooks.d.

# OPTIONS

- _Trigger=_

    Event type that will trigger the hook. Values are "Lock", "Idle", "Sleep",
    "Shutdown", "Inactive".

- _InactiveSec=_

    Seconds of inactivity after which the hook is triggered.

- _ExecStart=_

    Command to execute when the hook is triggered.

- _ExecStop=_

    Command to execute when the trigger event ends. For "Lock", this is when the
    screen is unlocked. For "Idle" and "Inactive", this is when activity resumes.

# SEE ALSO

[sessiond(1)]({{< ref "/man/sessiond.1.md" >}}), [sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}})
