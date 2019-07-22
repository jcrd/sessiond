#include "../src/config.h"
#include "../src/hooks.h"

#include <locale.h>
#include <glib-2.0/glib.h>

struct Paths {
    gchar *config;
    gchar *hooksd;
};

typedef struct {
    Config c;
    gboolean success;
} ConfigFixture;

static void
config_fixture_set_up(ConfigFixture *f, gconstpointer user_data)
{
    const struct Paths *paths = (struct Paths *)user_data;
    f->success = config_load(paths->config, paths->hooksd, &f->c);
}

static void
config_fixture_tear_down(ConfigFixture *f, gconstpointer user_data)
{
    config_free(&f->c);
}

static void
test_load(ConfigFixture *f, gconstpointer user_data)
{
    g_assert_true(f->success);
}

static void
test_hooks(ConfigFixture *f, gconstpointer user_data)
{
    GPtrArray *hooks = f->c.hooks;
    g_assert_nonnull(hooks);

    for (int i = 0; i < hooks->len; i++) {
        const struct Hook *h = g_ptr_array_index(hooks, i);
        switch (i) {
            case 0:
                g_assert_cmpint(h->trigger, ==, HOOK_TRIGGER_IDLE);
                g_assert_cmpstr(h->exec_start[0], ==, "test");
                g_assert_cmpstr(h->exec_start[1], ==, "start");
                break;
            case 1:
                g_assert_cmpint(h->trigger, ==, HOOK_TRIGGER_SLEEP);
                g_assert_cmpstr(h->exec_stop[0], ==, "test");
                g_assert_cmpstr(h->exec_stop[1], ==, "stop");
                break;
            case 2:
                g_assert_cmpint(h->trigger, ==, HOOK_TRIGGER_INACTIVE);
                g_assert_cmpint(h->inactive_sec, ==, 10);
                g_assert_cmpstr(h->exec_start[0], ==, "/usr/bin/touch");
                g_assert_cmpstr(h->exec_start[1], ==, "/tmp/test_run_inactive");
                g_assert_cmpstr(h->exec_stop[0], ==, "/bin/rm");
                g_assert_cmpstr(h->exec_stop[1], ==, "/tmp/test_run_inactive");
                break;
            case 3:
                g_assert_cmpint(h->trigger, ==, HOOK_TRIGGER_LOCK);
                g_assert_cmpstr(h->exec_start[0], ==, "/usr/bin/touch");
                g_assert_cmpstr(h->exec_start[1], ==, "/tmp/test_run_lock");
                g_assert_cmpstr(h->exec_stop[0], ==, "/bin/rm");
                g_assert_cmpstr(h->exec_stop[1], ==, "/tmp/test_run_lock");
                break;
            default:
                break;
        }
    }
}

static void
test_run_inactive_start(ConfigFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_on_timeout(f->c.hooks, 10, TRUE);
        return;
    }

    g_assert_nonnull(f->c.hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_true(g_file_test("/tmp/test_run_inactive", G_FILE_TEST_EXISTS));
}

static void test_run_inactive_stop(ConfigFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_on_timeout(f->c.hooks, 10, FALSE);
        return;
    }

    g_assert_nonnull(f->c.hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_false(g_file_test("/tmp/test_run_inactive", G_FILE_TEST_EXISTS));
}

static void
test_run_lock_start(ConfigFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_run(f->c.hooks, HOOK_TRIGGER_LOCK, TRUE);
        return;
    }

    g_assert_nonnull(f->c.hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_true(g_file_test("/tmp/test_run_lock", G_FILE_TEST_EXISTS));
}

static void
test_run_lock_stop(ConfigFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_run(f->c.hooks, HOOK_TRIGGER_LOCK, FALSE);
        return;
    }

    g_assert_nonnull(f->c.hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_false(g_file_test("/tmp/test_run_lock", G_FILE_TEST_EXISTS));
}


int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    struct Paths paths;
    paths.config = g_test_build_filename(G_TEST_DIST, "hooks.conf", NULL);
    paths.hooksd = g_test_build_filename(G_TEST_DIST, "hooks.d", NULL);

#define TEST(name, func) \
    g_test_add("/hooks/" #name, ConfigFixture, &paths, \
            config_fixture_set_up, test_##func, config_fixture_tear_down)

    TEST(load, load);
    TEST(hooks, hooks);
    TEST(run-inactive/start, run_inactive_start);
    TEST(run-inactive/stop, run_inactive_stop);
    TEST(run-lock/start, run_lock_start);
    TEST(run-lock/stop, run_lock_stop);
#undef TEST

    int ret = g_test_run();

    g_free(paths.config);
    g_free(paths.hooksd);

    return ret;
}
