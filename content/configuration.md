---
title: Configuration
---

sessiond looks for a configuration file at
`$XDG_CONFIG_HOME/sessiond/sessiond.conf`
or falls back to `$HOME/.config/sessiond/sessiond.conf` if `$XDG_CONFIG_HOME`
is unset.

The default configuration is included at `/usr/share/sessiond/sessiond.conf`.

See [sessiond.conf(5)][1] for descriptions of the options.

[1]: {{< ref "/man/sessiond.conf.5.md" >}}

## Hooks

Hook files with the `.hook` suffix are read from
`$XDG_CONFIG_HOME/sessiond/hooks.d` or `$HOME/.config/sessiond/hooks.d`.

Hooks provide functionality similar to systemd targets but can also be triggered
by a period of inactivity.

See [sessiond-hooks(5)][2] for more information.

[2]: {{< ref "/man/sessiond-hooks.5.md" >}}
