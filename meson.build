project('sessiond', 'c', version : '0.1.0')

deps = [
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
  dependency('x11'),
  dependency('xi'),
  ]

dpms = get_option('dpms')

if dpms.enabled()
  deps += dependency('xext')
  add_project_arguments('-DDPMS', language : 'c')
endif

# alternative to vcs_tag that allows reuse of version
# see https://github.com/mesonbuild/meson/issues/3903
r = run_command('git', 'describe', '--dirty', '--tags', '--always')
if r.returncode() == 0
  version = r.stdout().strip()
else
  version = meson.project_version()
endif

add_project_arguments('-DVERSION="@0@"'.format(version), language : 'c')

# generate dbus sources
gdbus_codegen = find_program('gdbus-codegen')

dbus_srcs = custom_target('dbus sources',
  command : [gdbus_codegen,
    '--generate-c-code', 'dbus-gen',
    '--interface-prefix', 'org.sessiond.session1.',
    '--c-namespace', 'DBus',
    '@INPUT@'],
  input : ['dbus/org.sessiond.session1.Session.xml'],
  output : ['dbus-gen.c', 'dbus-gen.h'])

srcs = [
  dbus_srcs,
  'src/backlight.c',
  'src/common.c',
  'src/config.c',
  'src/dbus-logind.c',
  'src/dbus-systemd.c',
  'src/dbus-server.c',
  'src/hooks.c',
  'src/keyfile.c',
  'src/sessiond.c',
  'src/timeline.c',
  'src/xsource.c',
  ]

executable('sessiond', sources : srcs, dependencies : deps, install : true)

executable('sessiond-sysfs-writer', 'src/helper/sessiond-sysfs-writer.c')

# install setuid helper
meson.add_install_script('scripts/install_sysfs_writer.sh')

install_data('sessiond.conf',
  install_dir : join_paths(get_option('datadir'), meson.project_name()))

install_data('sessionctl', install_dir : get_option('bindir'))

install_data('sessiond.desktop',
  install_dir : join_paths(get_option('datadir'), 'xsessions'))

units = [
  'systemd/graphical-idle.target',
  'systemd/graphical-unidle.target',
  'systemd/graphical-lock.target',
  'systemd/graphical-unlock.target',
  'systemd/user-sleep.target',
  'systemd/user-shutdown.target',
  'systemd/sessiond-session.target',
  'systemd/sessiond.service',
  ]

install_data(sources : units,
  install_dir : join_paths(get_option('libdir'), 'systemd/user'))

manpages = {
  'sessiond': '1',
  'sessiond.conf': '5',
  'sessiond-hooks': '5',
  }

# install manpages built with pod2man
mandir = get_option('mandir')
pod2man = find_program('pod2man')

foreach name, section : manpages
  manpage = '.'.join([name, section])
  custom_target(
    'manpage @0@'.format(manpage),
    input : join_paths('man', '@0@.pod'.format(manpage)),
    output : manpage,
    command : [
      pod2man,
      '--name=@0@'.format(name),
      '--center=@0@'.format(name),
      '--section=@0@'.format(section),
      '--release=@0@'.format(version),
      '@INPUT@', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(mandir, 'man@0@'.format(section)))
endforeach

subdir('test')
