Name: sessiond
Version: 0.6.1
Release: 1%{?dist}
Summary: Standalone X11 session manager for logind

License: GPLv3+
URL: https://github.com/jcrd/sessiond
Source0: https://github.com/jcrd/sessiond/archive/v0.6.1.tar.gz

BuildRequires: meson
BuildRequires: gcc
BuildRequires: perl
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(libudev)
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xi)
BuildRequires: pkgconfig(xext)
BuildRequires: pkgconfig(wireplumber-0.4)
BuildRequires: python3-devel
BuildRequires: python3-setuptools

Requires: python3-dbus

%description
sessiond is a standalone X session manager that reports the idle status of a
graphical session to systemd-logind. It can be used alongside a window manager
or desktop environment that does not provide its own session management.

%prep
%setup

%build
%meson --libdir lib
%meson_build

cd python-sessiond
%py3_build

%install
%meson_install

cd python-sessiond
%py3_install

%check
%meson_test

%files
%license LICENSE
%doc README.md
%{_bindir}/sessionctl
%{_bindir}/sessiond
%{_bindir}/sessiond-inhibit
/usr/lib/systemd/user/graphical-idle.target
/usr/lib/systemd/user/graphical-lock.target
/usr/lib/systemd/user/graphical-unidle.target
/usr/lib/systemd/user/graphical-unlock.target
/usr/lib/systemd/user/sessiond-session.target
/usr/lib/systemd/user/sessiond.service
/usr/lib/systemd/user/user-shutdown.target
/usr/lib/systemd/user/user-sleep.target
/usr/lib/systemd/user/user-sleep-finished.target
%{_mandir}/man1/sessionctl.1.gz
%{_mandir}/man1/sessiond-inhibit.1.gz
%{_mandir}/man1/sessiond.1.gz
%{_mandir}/man5/sessiond-hooks.5.gz
%{_mandir}/man5/sessiond.conf.5.gz
%{_mandir}/man8/sessiond-dbus.8.gz
%{_datadir}/sessiond/sessiond.conf
%{_datadir}/xsessions/sessiond.desktop

%{python3_sitelib}/%{name}-*.egg-info/
%{python3_sitelib}/%{name}.py
%{python3_sitelib}/__pycache__/%{name}.*

%changelog
* Wed Jan 26 2022 James Reed <james@twiddlingbits.net> - 0.6.1-1
- Release v0.6.1 hotfix

* Mon Jan 24 2022 James Reed <james@twiddlingbits.net> - 0.6.0-3
- Add missing file declaration
- Third time's a charm...

* Mon Jan 24 2022 James Reed <james@twiddlingbits.net> - 0.6.0-2
- Add missing build requirements

* Mon Jan 24 2022 James Reed <james@twiddlingbits.net> - 0.6.0-1
- Release v0.6.0

* Mon Apr 12 2021 James Reed <james@twiddlingbits.net> - 0.5.0-1
- Release v0.5.0

* Fri Apr  2 2021 James Reed <jcrd@tuta.io> - 0.4.0-1
- Release v0.4.0

* Thu Dec 31 2020 James Reed <jcrd@tuta.io> - 0.3.1-1
- Release v0.3.1

* Sun Nov 1 2020 James Reed <jcrd@tuta.io> - 0.3.0-1
- Release v0.3.0

* Wed Jun 17 2020 James Reed <jcrd@tuta.io> - 0.2.0-1
- Release v0.2.0

* Sat May 23 2020 James Reed <jcrd@tuta.io> - 0.1.0-2
- Use pkgconfig in build requires

* Mon May 11 2020 James Reed <jcrd@tuta.io> - 0.1.0
- Initial package
