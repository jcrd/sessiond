---
title: sessiond-dbus.8
---

# NAME

sessiond-dbus - sessiond DBus service information

# SYNOPSIS

DBus service information.

# DESCRIPTION

sessiond provides a DBus service on the session bus at the well-known name
_org.sessiond.session1_.

## Session interface

The **/org/sessiond/session1** object implements the
**org.sessiond.session1.Session** interface, which exposes the following
methods, properties, and signals:

### METHODS

- **Lock**

    Lock the session. Returns an error if the session is already locked.

- **Unlock**

    Unlock the session. Returns an error if the session is not locked.

- **Inhibit**

    Inhibit inactivity. The session will always be considered active if at least
    one inhibitor is running. Takes two arguments:

    - _who_

        A string describing who is inhibiting.

    - _why_

        A string describing why the inhibitor is running.

    Returns a unique ID used to stop the inhibitor.

- **Uninhibit**

    Stop an inhibitor. Takes one argument:

    - _id_

        The unique ID of the inhibitor to stop.

    Returns an error if the ID is not valid or does not exist.

- **StopInhibitors**

    Stop running inhibitors. Returns the number of inhibitors stopped.

- **ListInhibitors**

    List running inhibitors. Returns a dictionary mapping IDs to tuples of the
    creation timestamp and _who_ and _why_ strings.

### PROPERTIES

- **InhibitedHint**

    The inhibited state of the session.

- **LockedHint**

    The locked state of the session.

- **IdleHint**

    The idle state of the session.

- **IdleSinceHint**

    The timestamp of the last change to **IdleHint**.

- **IdleSinceHintMonotonic**

    The timestamp of the last change to **IdleHint** in monotonic time.

- **Backlights**

    An array of object paths to exported Backlights.

- **AudioSinks**

    An array of object paths to exported AudioSinks.

- **DefaultAudioSink**

    Object path to the default AudioSink.

- **Version**

    The version of sessiond.

### SIGNALS

- **Lock**

    Emitted when the session is locked. **LockedHint** will be true.

- **Unlock**

    Emitted when the session is unlocked. **LockedHint** will be false.

- **Idle**

    Emitted when the session becomes idle. **IdleHint** will be true.

- **Active**

    Emitted when activity resumes in an inactive session.
    **IdleHint** will be false.

- **Inactive** **seconds**

    Emitted when the session becomes inactive, with the **seconds** argument being
    the number of seconds since activity. Its value will be equal to either the
    _IdleSec_ or _DimSec_ configuration option (see [sessiond.conf(5)]({{< ref "/man/sessiond.conf.5.md" >}})), or the
    _InactiveSec_ option of a hook with an **Inactive** trigger
    (see [sessiond-hooks(5)]({{< ref "/man/sessiond-hooks.5.md" >}})).

- **PrepareForSleep** **state**

    Emitted before and after system sleep, with the **state** argument being true
    and false respectively.

- **PrepareForShutdown** **state**

    Emitted before and after system shutdown, with the **state** argument being true
    and false respectively.

- **AddBacklight** **path**

    Emitted when a backlight is added, with **path** being the object path of the
    new backlight.

- **RemoveBacklight** **path**

    Emitted when a backlight is removed, with **path** being the old object path of
    the backlight.

- **AddAudioSink** **path**

    Emitted when an audio sink is added, with **path** being the object path of the
    new audio sink.

- **RemoveAudioSink** **path**

    Emitted when an audio sink is removed, with **path** being the old object path of
    the audio sink.

- **ChangeDefaultAudioSink** **path**

    Emitted when the default audio sink changes, with **path** being the object path
    of the default audio sink.

## Backlight interface

The **/org/sessiond/session1/backlight/\*** objects implement the
**org.sessiond.session1.Backlight** interface, which exposes the following
methods and properties:

### METHODS

- **SetBrightness**

    Set the brightness of the backlight. Takes one argument:

    - _brightness_

        An unsigned integer value.

    Returns an error if unable to set brightness.

- **IncBrightness**

    Increment the brightness of the backlight. Takes one argument:

    - _value_

        An integer value added to the backlight's current brightness.

    Returns the new brightness value or an error if unable to set brightness.

### PROPERTIES

- **Online**

    True if the backlight is online, false otherwise.

- **DevPath**

    Path to the backlight device without the sys mount point.

- **Name**

    Name of the backlight.

- **Subsystem**

    Subsystem to which the backlight belongs. Possible values are: "backlight" or
    "leds".

- **SysPath**

    Path to the device via sys mount point. Format is:
    "/sys/class/_Subsystem_/_Name_".

- **Brightness**

    Current brightness of backlight.

- **MaxBrightness**

    Max brightness of backlight.

## AudioSink interface

The **/org/sessiond/session1/audiosink/\*** objects implement the
**org.sessiond.session1.AudioSink** interface, which exposes the following
methods, properties, and signals:

### METHODS

- **SetVolume**

    Set the volume of the audio sink. Takes one argument:

    - _volume_

        A double value.

    Returns an error if unable to set volume.

- **IncVolume**

    Increment the volume of the audio sink. Takes one argument:

    - _value_

        A double value added to the audio sink's current volume.

    Returns the new volume value or an error if unable to set volume.

- **SetMute**

    Set the mute state of the audio sink. Takes one argument:

    - _mute_

        A boolean value indicating the mute state.

    Returns an error if unable to set mute state.

- **ToggleMute**

    Toggle the mute state of the audio sink.
    Returns the new mute state or an error if unable to set mute state.

### PROPERTIES

- **Id**

    ID of the audio sink.

- **Name**

    Name of the audio sink.

- **Mute**

    Mute state of the audio sink.

- **Volume**

    Volume of the audio sink.

### SIGNALS

- **ChangeMute** **mute**

    Emitted when the mute state changes, with the **mute** argument being the new
    mute state.

- **ChangeVolume** **volume**

    Emitted when the volume changes, with the **volume** argument being the new
    volume value.

# INTROSPECTION

- For complete introspection data, use [gdbus(1)](https://www.commandlinux.com/man-page/man1/gdbus.1.html):

    **gdbus** introspect --session --dest _org.sessiond.session1_ --object-path
    _/org/sessiond/session1_

# AUTHOR

James Reed <jcrd@sessiond.org>

# REPORTING BUGS

Bugs and issues can be reported here: [https://github.com/jcrd/sessiond/issues](https://github.com/jcrd/sessiond/issues)

# COPYRIGHT

Copyright 2018-2020 James Reed. sessiond is licensed under the
GNU General Public License v3.0 or later.

# SEE ALSO

[gdbus(1)](https://www.commandlinux.com/man-page/man1/gdbus.1.html), [systemd-logind.service(8)](https://www.commandlinux.com/man-page/man8/systemd-logind.service.8.html)
