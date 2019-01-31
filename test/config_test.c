#include "../src/config.h"
#include "../src/xsource.h"

#include <locale.h>
#include <glib-2.0/glib.h>

typedef struct {
    Config c;
    gboolean success;
} ConfigFixture;

static void
config_fixture_set_up(ConfigFixture *f, gconstpointer user_data)
{
    f->success = config_load(&f->c, (const gchar *)user_data);
}

static void
config_fixture_tear_down(ConfigFixture *f, gconstpointer user_data)
{
    config_free(&f->c);
}

static void
test_load(ConfigFixture *f, gconstpointer user_data)
{
    const guint input_mask = INPUT_TYPE_MASK(RawKeyRelease) \
                             | INPUT_TYPE_MASK(RawButtonRelease);
    const Config *c = &f->c;

    g_assert_true(f->success);

    g_assert_cmpuint(c->input_mask, ==, input_mask);
    g_assert_cmpuint(c->idle_sec, ==, 600);
    g_assert_false(c->on_idle);
    g_assert_false(c->on_sleep);
    g_assert_false(c->bl_enable);
    g_assert_cmpstr(c->bl_name, ==, "/config/load/backlight");
    g_assert_cmpuint(c->dim_sec, ==, 600);
    g_assert_cmpuint(c->dim_percent, ==, 100);
    g_assert_false(c->dpms_enable);
    g_assert_cmpuint(c->standby_sec, ==, 60);
    g_assert_cmpuint(c->suspend_sec, ==, 61);
    g_assert_cmpuint(c->off_sec, ==, 62);
}

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    gchar *conf_load = g_test_build_filename(G_TEST_DIST,
            "config_load.conf", NULL);

    g_test_add("/config/load", ConfigFixture, conf_load,
            config_fixture_set_up, test_load, config_fixture_tear_down);

    int ret = g_test_run();

    g_free(conf_load);

    return ret;
}
