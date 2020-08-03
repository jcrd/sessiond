# This project is licensed under the MIT License.

"""
.. module:: sessiond
   :synopsis: Interface to the sessiond DBus service.

.. moduleauthor:: James Reed <jcrd@sessiond.org>
"""

import dbus

BUS_NAME = 'org.sessiond.session1'


class DBusIFace:
    """
    Base interface to the sessiond DBus service.

    :param path: DBus object path
    :param iface: DBus interface name
    """
    bus = dbus.SessionBus()

    @staticmethod
    def convert(val):
        """
        Convert a DBus-typed value to its python-type.

        :param val: Value to convert
        :return: The python-typed value
        """
        if isinstance(val, (dbus.String, dbus.ObjectPath)):
            return str(val)
        if isinstance(val, dbus.Boolean):
            return bool(val)
        if isinstance(val, dbus.UInt64):
            return int(val)
        if isinstance(val, dbus.Array):
            return list(map(DBusIFace.convert, list(val)))

        return None

    def __init__(self, path, iface):
        self.obj = DBusIFace.bus.get_object(BUS_NAME, path)
        self.iface = '{}.{}'.format(BUS_NAME, iface)
        self.props = dbus.Interface(self.obj,
                                    dbus_interface='org.freedesktop.DBus.Properties')
        self.interface = dbus.Interface(self.obj, dbus_interface=self.iface)

    def get_properties(self):
        """
        Get all DBus properties and values.

        :return: A dictionary mapping property names to values
        """
        return {DBusIFace.convert(k): DBusIFace.convert(v) for k, v in \
                self.props.GetAll(self.iface).items()}

    def get_property(self, name):
        """
        Get a DBus property's value.

        :param name: The property's name
        :return: The property's value
        """
        return DBusIFace.convert(self.props.Get(self.iface, name))


class Backlight(DBusIFace):
    """
    An interface to a sessiond Backlight object.

    :param name: The backlight's name
    """
    def __init__(self, name):
        self.name = name
        path = '/org/sessiond/session1/backlight/{}'.format(self.name)
        super().__init__(path, 'Backlight')

    def set_brightness(self, v):
        """
        Set the backlight's brightness.

        :param v: Brightness value
        :raises dbus.exception.DBusException: Raised if unable to set brightness
        """
        self.interface.SetBrightness(v)

    def inc_brightness(self, v):
        """
        Increment the backlight's brightness.

        :param v: Brightness value by which to increment
        :return: The new brightness value
        :raises dbus.exception.DBusException: Raised if unable to set brightness
        """
        return self.interface.IncBrightness(v)

    @property
    def online(self):
        """
        Backlight's online status.

        :return: `True` if online, `False` otherwise
        """
        return self.get_property('Online')

    @property
    def subsystem(self):
        """
        Backlight's subsystem.

        :return: Name of backlight's subsystem
        """
        return self.get_property('Subsystem')

    @property
    def sys_path(self):
        """
        Backlight device's /sys path.

        :return: Path to the device via sys mount point
        """
        return self.get_property('SysPath')

    @property
    def dev_path(self):
        """
        Backlight device's path.

        :return: Path to the backlight device without the sys mount point
        """
        return self.get_property('DevPath')

    @property
    def max_brightness(self):
        """
        Backlight's maximum brightness.

        :return: Maximum brightness value
        """
        return self.get_property('MaxBrightness')

    @property
    def brightness(self):
        """
        Backlight's brightness.

        :return: Brightness value
        """
        return self.get_property('Brightness')


class Session(DBusIFace):
    """
    An interface to sessiond's Session.
    """
    def __init__(self):
        super().__init__('/org/sessiond/session1', 'Session')

    def inhibit(self, who="", why=""):
        """
        Add an inhibitor.

        :param who: A string describing who is inhibiting
        :param why: A string describing why this inhibitor is running
        :return: The inhibitor's ID
        """
        return self.interface.Inhibit(who, why)

    def uninhibit(self, id):
        """
        Remove an inhibitor.

        :param id: The inhibitor's ID
        :raises dbus.exception.DBusException: Raised if the ID is not valid or \
        does not exist
        """
        self.interface.Uninhibit(id)

    def stop_inhibitors(self):
        """
        Stop running inhibitors.

        :return: The number of inhibitors stopped
        """
        return self.interface.StopInhibitors()

    def list_inhibitors(self):
        """
        List running inhibitors.

        :return: A dictionary mapping IDs to tuples of 'who' and 'why' strings
        """
        return {str(k): (str(s[0]), str(s[1])) for k, s \
                in self.interface.ListInhibitors().items()}

    def lock(self):
        """
        Lock the session.

        :raises dbus.exception.DBusException: Raised if the session is already \
        locked
        """
        self.interface.Lock()

    def unlock(self):
        """
        Unlock the session.

        :raises dbus.exception.DBusException: Raised if the session is not \
        locked
        """
        self.interface.Unlock()

    @property
    def backlights(self):
        """
        List of backlights.

        :return: A list of Backlight DBus object paths
        """
        return self.get_property('Backlights')

    @property
    def idle_hint(self):
        """
        Session idle hint.

        :return: `True` if session is idle, `False` otherwise
        """
        return self.get_property('IdleHint')

    @property
    def inhibited_hint(self):
        """
        Session inhibited hint.

        :return: `True` if session is inhibited, `False` otherwise
        """
        return self.get_property('InhibitedHint')

    @property
    def locked_hint(self):
        """
        Session locked hint.

        :return: `True` if session is locked, `False` otherwise
        """
        return self.get_property('LockedHint')

    @property
    def version(self):
        """
        sessiond version.

        :return: sessiond's version string
        """
        return self.get_property('Version')

    @property
    def idle_since_hint(self):
        """
        The timestamp of the last change to IdleHint.

        :return: The timestamp
        """
        return self.get_property('IdleSinceHint')

    @property
    def idle_since_hint_monotonic(self):
        """
        The timestamp of the last change to IdleHint in monotonic time.

        :return: The timestamp
        """
        return self.get_property('IdleSinceHintMonotonic')
