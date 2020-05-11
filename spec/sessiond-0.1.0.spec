Name: sessiond
Version: 0.1.0
Release: 1%{?dist}
Summary: Standalone X11 session manager for logind

License: GPLv3+
URL: https://github.com/jcrd/sessiond
Source0: https://github.com/jcrd/sessiond/archive/v0.1.0.tar.gz

BuildRequires: meson
BuildRequires: gcc
BuildRequires: perl
BuildRequires: glib2-devel
BuildRequires: systemd-devel
BuildRequires: libXi-devel
BuildRequires: libX11-devel
BuildRequires: libXext-devel

Requires: perl
Requires: glib2
Requires: bash
Requires: grep
Requires: coreutils

%description
sessiond is a standalone X session manager that reports the idle status of a
graphical session to systemd-logind. It can be used alongside a window manager
or desktop environment that does not provide its own session management.

%prep
%setup

%build
%meson --libdir lib
%meson_build

%install
%meson_install

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
%{_mandir}/man1/sessionctl.1.gz
%{_mandir}/man1/sessiond-inhibit.1.gz
%{_mandir}/man1/sessiond.1.gz
%{_mandir}/man5/sessiond-hooks.5.gz
%{_mandir}/man5/sessiond.conf.5.gz
%{_datadir}/sessiond/sessiond.conf
%{_datadir}/xsessions/sessiond.desktop

%changelog
* Mon May 11 2020 James Reed <jcrd@tuta.io> - 0.1.0
- Initial package
