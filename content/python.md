---
title: Python API
---

{{< toc >}}
# python-sessiond API


### class sessiond.AudioSink(id)
An interface to a sessiond AudioSink object.


* **Parameters**

    **id** – The audio sink’s ID



#### property id()
Audio sink’s ID.


* **Returns**

    Audio sink’s ID



#### inc_volume(v)
Increment the audio sink’s volume.


* **Parameters**

    **v** – Volume value by which to increment



* **Returns**

    The new volume value



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to increment volume



#### property mute()
Audio sink’s mute state.


* **Returns**

    True if muted, False otherwise



#### property name()
Audio sink’s name.


* **Returns**

    Audio sink’s name



#### set_mute(m)
Set the audio sink’s mute state.


* **Parameters**

    **m** – Mute state



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to set mute state



#### set_volume(v)
Set the audio sink’s volume.


* **Parameters**

    **v** – Volume value



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to set volume



#### toggle_mute()
Toggle the audio sink’s mute state.


* **Returns**

    The new mute state



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to toggle mute state



#### property volume()
Audio sink’s volume.


* **Returns**

    Audio sink’s volume



### class sessiond.Backlight(name)
An interface to a sessiond Backlight object.


* **Parameters**

    **name** – The backlight’s name



#### property brightness()
Backlight’s brightness.


* **Returns**

    Brightness value



#### property dev_path()
Backlight device’s path.


* **Returns**

    Path to the backlight device without the sys mount point



#### inc_brightness(v)
Increment the backlight’s brightness.


* **Parameters**

    **v** – Brightness value by which to increment



* **Returns**

    The new brightness value



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to set brightness



#### property max_brightness()
Backlight’s maximum brightness.


* **Returns**

    Maximum brightness value



#### property online()
Backlight’s online status.


* **Returns**

    True if online, False otherwise



#### set_brightness(v)
Set the backlight’s brightness.


* **Parameters**

    **v** – Brightness value



* **Raises**

    **dbus.exception.DBusException** – Raised if unable to set brightness



#### property subsystem()
Backlight’s subsystem.


* **Returns**

    Name of backlight’s subsystem



#### property sys_path()
Backlight device’s /sys path.


* **Returns**

    Path to the device via sys mount point



### class sessiond.DBusIFace(path, iface)
Base interface to the sessiond DBus service.


* **Parameters**

    
    * **path** – DBus object path


    * **iface** – DBus interface name



#### static convert(val)
Convert a DBus-typed value to its python-type.


* **Parameters**

    **val** – Value to convert



* **Returns**

    The python-typed value



#### get_properties()
Get all DBus properties and values.


* **Returns**

    A dictionary mapping property names to values



#### get_property(name)
Get a DBus property’s value.


* **Parameters**

    **name** – The property’s name



* **Returns**

    The property’s value



### class sessiond.Session()
An interface to sessiond’s Session.


#### property audiosinks()
List of audio sinks.


* **Returns**

    A list of AudioSink DBus object paths



#### property backlights()
List of backlights.


* **Returns**

    A list of Backlight DBus object paths



#### property default_audiosink()
Get the default audio sink.


* **Returns**

    The default audio sink’s DBus object path



#### property idle_hint()
Session idle hint.


* **Returns**

    True if session is idle, False otherwise



#### property idle_since_hint()
The timestamp of the last change to IdleHint.


* **Returns**

    The timestamp



#### property idle_since_hint_monotonic()
The timestamp of the last change to IdleHint in monotonic time.


* **Returns**

    The timestamp



#### inhibit(who='', why='')
Add an inhibitor.


* **Parameters**

    
    * **who** – A string describing who is inhibiting


    * **why** – A string describing why this inhibitor is running



* **Returns**

    The inhibitor’s ID



#### property inhibited_hint()
Session inhibited hint.


* **Returns**

    True if session is inhibited, False otherwise



#### list_inhibitors()
List running inhibitors.


* **Returns**

    A dictionary mapping IDs to tuples of the creation timestamp         and ‘who’ and ‘why’ strings



#### lock()
Lock the session.


* **Raises**

    **dbus.exception.DBusException** – Raised if the session is already         locked



#### property locked_hint()
Session locked hint.


* **Returns**

    True if session is locked, False otherwise



#### stop_inhibitors()
Stop running inhibitors.


* **Returns**

    The number of inhibitors stopped



#### uninhibit(id)
Remove an inhibitor.


* **Parameters**

    **id** – The inhibitor’s ID



* **Raises**

    **dbus.exception.DBusException** – Raised if the ID is not valid or         does not exist



#### unlock()
Unlock the session.


* **Raises**

    **dbus.exception.DBusException** – Raised if the session is not         locked



#### property version()
sessiond version.


* **Returns**

    sessiond’s version string
