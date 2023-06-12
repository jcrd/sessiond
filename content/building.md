---
title: Building
---

## Dependencies

* [meson][1] >= 0.47.0 [*build*]
* perl [*build*]
* glib >= 2.52 [*build*]
* libx11 [*build*]
* libxi [*build*]
* libxext (optional, for DPMS support) [*build*]
* libwireplumber-0.4 (optional, for audio sink support) [*build*]
* python3-setuptools [*build*]
* python3 [*runtime*]
* dbus-python [*runtime*]

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

[1]: https://mesonbuild.com

## Testing

Run tests with `meson test`.

## Installing

After building sessiond, `ninja install` can be used to install it.

The `sessiond` Python package must also be installed. Run:
```
cd python-sessiond
python3 setup.py install
```
