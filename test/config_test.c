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
    gchar *path = (gchar *)user_data;
    f->success = config_load(path, NULL, &f->c);
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
test_input(ConfigFixture *f, gconstpointer user_data)
{
    const guint input_mask = INPUT_TYPE_MASK(RawKeyRelease) \
                             | INPUT_TYPE_MASK(RawButtonRelease);
    const Config *c = &f->c;
    g_assert_cmpuint(c->input_mask, ==, input_mask);
    g_assert_cmpuint(c->idle_sec, ==, 600);
}

static void
test_idle(ConfigFixture *f, gconstpointer user_data)
{
    const Config *c = &f->c;
    g_assert_false(c->on_idle);
    g_assert_false(c->on_sleep);
}

static void
test_dpms(ConfigFixture *f, gconstpointer user_data)
{
    const Config *c = &f->c;
    g_assert_false(c->dpms_enable);
    g_assert_cmpuint(c->standby_sec, ==, 60);
    g_assert_cmpuint(c->suspend_sec, ==, 61);
    g_assert_cmpuint(c->off_sec, ==, 62);
}

static void
test_backlights(ConfigFixture *f, gconstpointer user_data)
{
    GHashTable *bls = f->c.backlights;
    g_assert_nonnull(bls);

    struct BacklightConf *bl = g_hash_table_lookup(bls, "/sys/class/backlight/1");
    g_assert_nonnull(bl);
    g_assert_cmpuint(bl->dim_sec, ==, 600);
    g_assert_cmpint(bl->dim_percent, ==, 100);
    g_assert_cmpint(bl->dim_value, ==, -1);

    struct BacklightConf *led = g_hash_table_lookup(bls, "/sys/class/leds/1");
    g_assert_nonnull(led);
    g_assert_cmpuint(led->dim_sec, ==, 600);
    g_assert_cmpint(led->dim_percent, ==, -1);
    g_assert_cmpint(led->dim_value, ==, 1);
}

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    gchar *path = g_test_build_filename(G_TEST_DIST, "test.conf", NULL);

#define TEST(name) \
    g_test_add("/config/" #name, ConfigFixture, path, \
            config_fixture_set_up, test_##name, config_fixture_tear_down)

    TEST(load);
    TEST(input);
    TEST(idle);
    TEST(dpms);
    TEST(backlights);
#undef TEST

    int ret = g_test_run();

    g_free(path);

    return ret;
}
