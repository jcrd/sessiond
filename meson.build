project('sessiond', 'c',
  version : 'v0.6.0',
  meson_version : '>=0.47.0')

deps = [
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
  dependency('x11'),
  dependency('xi'),
  dependency('libudev'),
  ]

# alternative to vcs_tag that allows reuse of version
# see https://github.com/mesonbuild/meson/issues/3903
version = meson.project_version()
git = find_program('git', required : false)
if git.found()
  r = run_command('git', 'describe', '--dirty', '--tags', '--always')
  if r.returncode() == 0
    version = r.stdout().strip()
  endif
endif

add_project_arguments('-DVERSION="@0@"'.format(version), language : 'c')
add_project_arguments('-DG_LOG_USE_STRUCTURED', language : 'c')
add_project_arguments('-DPREFIX="@0@"'.format(get_option('prefix')), language : 'c')

if get_option('dpms').enabled()
  deps += dependency('xext')
  add_project_arguments('-DDPMS', language : 'c')
endif

if get_option('backlight_helper').enabled()
  add_project_arguments('-DBACKLIGHT_HELPER', language : 'c')
  executable('sessiond-sysfs-writer', 'src/helper/sessiond-sysfs-writer.c')

  # install setuid helper
  meson.add_install_script('scripts/install_sysfs_writer.sh')
endif

# generate dbus sources
gdbus_codegen = find_program('gdbus-codegen')

dbus_srcs = custom_target('dbus sources',
  command : [gdbus_codegen,
    '--generate-c-code', 'dbus-gen',
    '--interface-prefix', 'org.sessiond.session1.',
    '--c-namespace', 'DBus',
    '@INPUT@'],
  input : ['dbus/org.sessiond.session1.xml'],
  output : ['dbus-gen.c', 'dbus-gen.h'])

srcs = [
  dbus_srcs,
  'src/backlight.c',
  'src/common.c',
  'src/config.c',
  'src/dbus-logind.c',
  'src/dbus-systemd.c',
  'src/dbus-server.c',
  'src/dbus-backlight.c',
  'src/hooks.c',
  'src/sessiond.c',
  'src/timeline.c',
  'src/xsource.c',
  'src/toml/toml.c',
  ]

if get_option('wireplumber').enabled()
  deps += dependency('wireplumber-0.4')
  srcs += 'src/dbus-audiosink.c'
  srcs += 'src/wireplumber.c'
  add_project_arguments('-DWIREPLUMBER', language : 'c')
endif

executable('sessiond', sources : srcs, dependencies : deps, install : true)

install_data('sessiond.conf',
  install_dir : join_paths(get_option('datadir'), meson.project_name()))

install_data('sessionctl', install_dir : get_option('bindir'))
install_data('sessiond-inhibit', install_dir : get_option('bindir'))

install_data('sessiond.desktop',
  install_dir : join_paths(get_option('datadir'), 'xsessions'))

units = [
  'systemd/graphical-idle.target',
  'systemd/graphical-unidle.target',
  'systemd/graphical-lock.target',
  'systemd/graphical-unlock.target',
  'systemd/user-sleep.target',
  'systemd/user-sleep-finished.target',
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
  'sessiond-dbus': '8',
  'sessionctl': '1',
  'sessiond-inhibit': '1',
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
