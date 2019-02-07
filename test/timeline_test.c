#include "../src/timeline.h"

#include <locale.h>
#include <glib-2.0/glib.h>

typedef struct {
    GMainContext *ctx;
    GMainLoop *loop;
    Timeline tl;
    GArray *times;
} TimelineFixture;

static void
tl_func(guint timeout, gboolean state, gconstpointer user_data)
{
    TimelineFixture *f = (TimelineFixture *)user_data;
    double t = g_test_timer_elapsed();

    g_array_append_val(f->times, t);

    if (!timeline_pending_timeouts(&f->tl))
        g_main_loop_quit(f->loop);
}

static void
tl_fixture_set_up(TimelineFixture *f, gconstpointer user_data)
{
    f->ctx = g_main_context_new();
    f->loop = g_main_loop_new(f->ctx, FALSE);
    f->tl = timeline_new(f->ctx, tl_func, f);
    f->times = g_array_new(FALSE, FALSE, sizeof(double));
}

static void
tl_fixture_tear_down(TimelineFixture *f, gconstpointer user_data)
{
    g_array_unref(f->times);
    timeline_free(&f->tl);
    g_main_loop_unref(f->loop);
    g_main_context_unref(f->ctx);
}

static void
test_timeline_timeout(TimelineFixture *f, gconstpointer user_data)
{
    guint timeouts[] = {1, 3, 5};

    for (guint i = 0; i < G_N_ELEMENTS(timeouts); i++)
        timeline_add_timeout(&f->tl, timeouts[i]);

    timeline_start(&f->tl);
    g_test_timer_start();
    g_main_loop_run(f->loop);
    timeline_stop(&f->tl);

    g_assert_cmpint(f->times->len, ==, G_N_ELEMENTS(timeouts));

    for (guint i = 0; i < f->times->len; i++)
        g_assert_cmpint(g_array_index(f->times, double, i), <=, timeouts[i]);
}

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/timeline/timeout", TimelineFixture, NULL,
            tl_fixture_set_up, test_timeline_timeout, tl_fixture_tear_down);

    return g_test_run();
}
