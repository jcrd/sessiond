Name: {{{ git_name name="sessiond" }}}
Version: {{{ git_version lead="$(git tag | sed -n 's/^v//p' | sort --version-sort -r | head -n1)" }}}
Release: 1%{?dist}
Summary: Standalone X11 session manager for logind

License: GPLv3+
URL: https://github.com/jcrd/sessiond
VCS: {{{ git_vcs }}}
Source0: {{{ git_pack }}}

BuildRequires: meson
BuildRequires: gcc
BuildRequires: perl
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(libudev)
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xi)
BuildRequires: python3-devel
BuildRequires: python3-setuptools

Requires: python3-dbus

%description
sessiond is a standalone X session manager that reports the idle status of a
graphical session to systemd-logind. It can be used alongside a window manager
or desktop environment that does not provide its own session management.

%prep
{{{ git_setup_macro }}}

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
%{_datadir}/sessiond/sessiond.conf
%{_datadir}/xsessions/sessiond.desktop

%{python3_sitelib}/%{name}-*.egg-info/
%{python3_sitelib}/%{name}.py
%{python3_sitelib}/__pycache__/%{name}.*

%changelog
{{{ git_dir_changelog }}}
