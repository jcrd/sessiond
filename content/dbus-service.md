---
title: DBus service
---

sessiond provides a DBus service on the session bus at the well-known name
**_org.sessiond.session1_**.

See [sessiond-dbus(8)][1] for descriptions of methods, properties, and signals.

For complete introspection data, use **gdbus**:

```
gdbus introspect --session --dest org.sessiond.session1 --object-path /org/sessiond/session1
```

[1]: {{< ref "/man/sessiond-dbus.8.md" >}}

## sessionctl

The `sessionctl` script is provided to run a sessiond session and interact with
its DBus service.

```
usage: sessionctl [-h]
                  {run,stop,status,lock,unlock,properties,backlight,audiosink,version}
                  ...

With no arguments, show session status.

positional arguments:
  {run,stop,status,lock,unlock,properties,backlight,audiosink,version}
    run                 Run session
    stop                Stop the running session
    status              Show session status
    lock                Lock the session
    unlock              Unlock the session
    properties          List session properties
    backlight           Interact with backlights
    audiosink           Interact with audio sinks
    version             Show sessiond version

options:
  -h, --help            show this help message and exit
```

See [sessionctl(1)][2] for more information.

[2]: {{< ref "/man/sessionctl.1.md" >}}
