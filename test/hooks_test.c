#include "../src/hooks.h"

#include <locale.h>
#include <glib-2.0/glib.h>

typedef struct {
    GPtrArray *hooks;
} HooksFixture;

static void
hooks_fixture_set_up(HooksFixture *f, gconstpointer user_data)
{
    f->hooks = hooks_load((const gchar *)user_data);
}

static void
hooks_fixture_tear_down(HooksFixture *f, gconstpointer user_data)
{
    hooks_free(f->hooks);
}

static void
test_load_inactive(HooksFixture *f, gconstpointer user_data)
{
    g_assert_nonnull(f->hooks);

    gboolean hook_found = FALSE;
    for (guint i = 0; i < f->hooks->len; i++) {
        const Hook *h = g_ptr_array_index(f->hooks, i);
        if (h->type != HOOK_TYPE_INACTIVE)
            continue;
        g_assert_cmpuint(h->inactive_sec, ==, 10);
        g_assert_cmpstr(h->exec_start[0], ==, "/usr/bin/touch");
        g_assert_cmpstr(h->exec_start[1], ==, "/tmp/test_run_inactive");
        g_assert_cmpstr(h->exec_stop[0], ==, "/bin/rm");
        g_assert_cmpstr(h->exec_stop[1], ==, "/tmp/test_run_inactive");
        hook_found = TRUE;
        break;
    }

    g_assert_true(hook_found);
}

static void
test_run_inactive_start(HooksFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_on_timeout(f->hooks, 10, TRUE);
        return;
    }

    g_assert_nonnull(f->hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_true(g_file_test("/tmp/test_run_inactive", G_FILE_TEST_EXISTS));
}

static void
test_run_inactive_stop(HooksFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_on_timeout(f->hooks, 10, FALSE);
        return;
    }

    g_assert_nonnull(f->hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_false(g_file_test("/tmp/test_run_inactive", G_FILE_TEST_EXISTS));
}

static void
test_run_lock_start(HooksFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_run(f->hooks, HOOK_TYPE_LOCK, TRUE);
        return;
    }

    g_assert_nonnull(f->hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_true(g_file_test("/tmp/test_run_lock", G_FILE_TEST_EXISTS));
}

static void
test_run_lock_stop(HooksFixture *f, gconstpointer user_data)
{
    if (g_test_subprocess()) {
        hooks_run(f->hooks, HOOK_TYPE_LOCK, FALSE);
        return;
    }

    g_assert_nonnull(f->hooks);
    g_test_trap_subprocess(NULL, 0, 0);
    g_assert_false(g_file_test("/tmp/test_run_lock", G_FILE_TEST_EXISTS));
}


int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    gchar *path = g_test_build_filename(G_TEST_DIST, "hooks.d", NULL);

    g_test_add("/hooks/load", HooksFixture, path,
            hooks_fixture_set_up, test_load_inactive, hooks_fixture_tear_down);

    g_test_add("/hooks/run-lock/start", HooksFixture, path,
            hooks_fixture_set_up, test_run_lock_start, hooks_fixture_tear_down);

    g_test_add("/hooks/run-lock/stop", HooksFixture, path,
            hooks_fixture_set_up, test_run_lock_stop, hooks_fixture_tear_down);

    g_test_add("/hooks/run-inactive/start", HooksFixture, path,
            hooks_fixture_set_up, test_run_inactive_start, hooks_fixture_tear_down);

    g_test_add("/hooks/run-inactive/stop", HooksFixture, path,
            hooks_fixture_set_up, test_run_inactive_stop, hooks_fixture_tear_down);

    int ret = g_test_run();

    g_free(path);

    return ret;
}
