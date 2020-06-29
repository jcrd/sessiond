import dbus

BUS_NAME = 'org.sessiond.session1'


class DBusIFace:
    bus = dbus.SessionBus()

    @staticmethod
    def convert(v):
        if isinstance(v, dbus.String):
            return str(v)
        elif isinstance(v, dbus.Boolean):
            return bool(v)
        elif isinstance(v, dbus.UInt64):
            return int(v)
        elif isinstance(v, dbus.Array):
            return list(v)

    def __init__(self, path, iface):
        self.obj = DBusIFace.bus.get_object(BUS_NAME, path)
        self.iface = '{}.{}'.format(BUS_NAME, iface)
        self.props = dbus.Interface(self.obj,
                                    dbus_interface='org.freedesktop.DBus.Properties')
        self.interface = dbus.Interface(self.obj, dbus_interface=self.iface)

    def get_properties(self):
        return {DBusIFace.convert(k): DBusIFace.convert(v) for k, v in \
                self.props.GetAll(self.iface).items()}

    def get_property(self, name):
        return DBusIFace.convert(self.props.Get(self.iface, name))


class Backlight(DBusIFace):
    def __init__(self, name):
        self.name = name
        path = '/org/sessiond/session1/backlight/{}'.format(self.name)
        super().__init__(path, 'Backlight')

    def set_brightness(self, v):
        self.interface.SetBrightness(v)

    def inc_brightness(self, v):
        return self.interface.IncBrightness(v)

    @property
    def online(self):
        return self.get_property('Online')

    @property
    def subsystem(self):
        return self.get_property('Subsystem')

    @property
    def sys_path(self):
        return self.get_property('SysPath')

    @property
    def dev_path(self):
        return self.get_property('DevPath')

    @property
    def max_brightness(self):
        return self.get_property('MaxBrightness')

    @property
    def brightness(self):
        return self.get_property('Brightness')


class Session(DBusIFace):
    def __init__(self):
        super().__init__('/org/sessiond/session1', 'Session')

    def inhibit(self, who="", why=""):
        return self.interface.Inhibit(who, why)

    def uninhibit(self, id):
        self.interface.Uninhibit(id)

    def stop_inhibitors(self):
        return self.interface.StopInhibitors()

    def list_inhibitors(self):
        def convert(s):
            return str(s[0]), str(s[1])
        return [convert(s) for s in self.interface.ListInhibitors()]

    def lock(self):
        self.interface.Lock()

    def unlock(self):
        self.interface.Unlock()

    def get_backlight(self, name):
        return Backlight(name)

    @property
    def backlights(self):
        return self.get_property('Backlights')

    @property
    def idle_hint(self):
        return self.get_property('IdleHint')

    @property
    def inhibited_hint(self):
        return self.get_property('InhibitedHint')

    @property
    def locked_hint(self):
        return self.get_property('LockedHint')

    @property
    def version(self):
        return self.get_property('Version')

    @property
    def idle_since_hint(self):
        return self.get_property('IdleSinceHint')

    @property
    def idle_since_hint_monotonic(self):
        return self.get_property('IdleSinceHintMonotonic')
