---
title: Getting started
---

## Installation

{{< tabs "installation" >}}

{{< tab "Packages" >}}
If a package is available for your distro, install it using the instructions below:

* Fedora ([copr][copr])

  ```
  dnf copr enable jcrd/sessiond
  dnf install sessiond
  ```

* Arch Linux ([AUR][aur])

  ```
  git clone https://aur.archlinux.org/sessiond.git
  cd sessiond
  makepkg -si
  ```

[copr]: https://copr.fedorainfracloud.org/coprs/jcrd/sessiond/
[aur]: https://aur.archlinux.org/packages/sessiond
{{< /tab >}}

{{< tab "From source" >}}
Follow the instructions below to build sessiond from source.
See [Building](/building/#dependencies) for a complete list of dependencies.

1. Download and extract the latest release, then enter the created directory:

```
curl -L https://github.com/jcrd/sessiond/archive/refs/tags/v0.6.1.tar.gz | tar -xz -C sessiond
cd sessiond
```

2. Initiate the build process with `meson` and `ninja`:

```
meson builddir
ninja -C builddir
```

3. Install the built package:

```
sudo ninja -C builddir install
```

4. Install the Python package:

```
cd python-sessiond
sudo python3 setup.py install
```

{{< /tab >}}

{{< /tabs >}}

## Usage

See [Usage][usage] for details about using sessiond.

Refer to [dovetail.service][dovetail] for a working example of integrating a window manager with sessiond.

Examples of services for use in a sessiond session can be found [here][services].

[usage]: {{< ref "/usage.md" >}}
[dovetail]: <https://github.com/jcrd/dovetail/blob/master/systemd/dovetail.service>
[services]: <https://gist.github.com/jcrd/c53a8446f1483b355606ed27299919cb>

## Configuration

sessiond does not require configuration but allows it to modify the default behavior.

See [Configuration][configuration] for more information.

[configuration]: {{< ref "/configuration.md" >}}
