g_test_env = [
  'G_MESSAGES_DEBUG=all',
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'XDG_CONFIG_HOME=@0@'.format(join_paths(meson.current_source_dir(), 'test')),
  ]

test(
  'test config loading',
  executable('config_test', [
    'config_test.c',
    '../src/config.c',
    '../src/keyfile.c',
    ], dependencies : deps),
  env : g_test_env,
  )

test(
  'test hooks',
  executable('hooks_test', [
    'hooks_test.c',
    '../src/hooks.c',
    '../src/keyfile.c',
    '../src/timeline.c',
    '../src/common.c',
    ], dependencies : deps),
  env : g_test_env,
  )

test(
  'test timeline',
  executable('timeline_test', [
    'timeline_test.c',
    '../src/timeline.c',
    ], dependencies : deps),
  env : g_test_env,
  )
