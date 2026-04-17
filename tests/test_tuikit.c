#include "c-tuikit/c-tuikit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            exit(1);                                                    \
        }                                                               \
    } while (0)

typedef struct {
    int remaining;
    int live;
} failing_alloc_t;

static void *failing_alloc(void *ctx, size_t size)
{
    failing_alloc_t *fa = ctx;

    if (fa->remaining-- == 0)
        return NULL;
    fa->live++;
    return malloc(size);
}

static void failing_free(void *ctx, void *ptr)
{
    failing_alloc_t *fa = ctx;

    fa->live--;
    free(ptr);
}

static void noop_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void noop_status(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void noop_timer(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void noop_fd(void *ctx, pnl_app_t *app, int fd)
{
    (void)ctx;
    (void)app;
    (void)fd;
}

static void noop_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    (void)ctx;
    (void)app;
    (void)canvas;
}

static void count_call(void *ctx)
{
    (*(int *)ctx)++;
}

static int decode(const char *bytes, size_t *out_used)
{
    int key = -1;

    *out_used = pnl_key_decode((const unsigned char *)bytes, strlen(bytes), &key);
    return key;
}

static void test_keys(void)
{
    size_t used;
    int key = -1;

    CHECK(decode("\x1b[A", &used) == PNL_KEY_UP && used == 3);
    CHECK(decode("\x1b[B", &used) == PNL_KEY_DOWN && used == 3);
    CHECK(decode("\x1bOC", &used) == PNL_KEY_RIGHT && used == 3);
    CHECK(decode("\x1b[D", &used) == PNL_KEY_LEFT && used == 3);
    CHECK(decode("\x1b", &used) == PNL_KEY_ESCAPE && used == 1);
    CHECK(decode("\x1bx", &used) == PNL_KEY_ESCAPE && used == 1);
    CHECK(decode("\x1b[1;5A", &used) == PNL_KEY_NONE && used == 6);
    CHECK(decode("\x1b[5~", &used) == PNL_KEY_PAGE_UP && used == 4);
    CHECK(decode("\x1b[6~", &used) == PNL_KEY_PAGE_DOWN && used == 4);
    CHECK(decode("\x1b[3~", &used) == PNL_KEY_DELETE && used == 4);
    CHECK(decode("\x1b[1~", &used) == PNL_KEY_HOME && used == 4);
    CHECK(decode("\x1b[4~", &used) == PNL_KEY_END && used == 4);
    CHECK(decode("\x1b[H", &used) == PNL_KEY_HOME && used == 3);
    CHECK(decode("\x1bOF", &used) == PNL_KEY_END && used == 3);
    CHECK(decode("\x1b[9~", &used) == PNL_KEY_NONE && used == 4);
    CHECK(decode("\x1b[15~", &used) == PNL_KEY_NONE && used == 5);
    CHECK(decode("\x7f", &used) == PNL_KEY_BACKSPACE && used == 1);
    CHECK(decode("\t", &used) == PNL_KEY_TAB && used == 1);
    CHECK(decode("\x1b[Z", &used) == PNL_KEY_BACKTAB && used == 3);
    CHECK(decode("\x1b]11;rgb:ffff/ffff/ffff\x1b\\q", &used) == PNL_KEY_NONE && used == 25);
    CHECK(decode("\x1b]11;rgb:0/0/0\x07q", &used) == PNL_KEY_NONE && used == 15);
    CHECK(decode("\x1b[?62;22c", &used) == PNL_KEY_NONE && used == 9);
    CHECK(decode("\x1b[<0;12;5M", &used) == PNL_KEY_CLICK && used == 10);
    CHECK(decode("\x1b[<0;12;5m", &used) == PNL_KEY_NONE && used == 10);
    CHECK(decode("\x1b[<64;1;1M", &used) == PNL_KEY_WHEEL_UP && used == 10);
    CHECK(decode("\x1b[<65;300;200M", &used) == PNL_KEY_WHEEL_DOWN && used == 14);
    CHECK(decode("\x1b[<2;3;4M", &used) == PNL_KEY_NONE && used == 9);
    CHECK(decode("\x1b[<0;0;4M", &used) == PNL_KEY_NONE && used == 9);
    CHECK(decode("\x1b[<0;3M", &used) == PNL_KEY_NONE && used == 7);
    CHECK(decode("\x08", &used) == PNL_KEY_BACKSPACE && used == 1);
    CHECK(decode("\r", &used) == PNL_KEY_ENTER && used == 1);
    CHECK(decode("\n", &used) == PNL_KEY_ENTER && used == 1);
    CHECK(decode("q", &used) == 'q' && used == 1);
    CHECK(decode("\x03", &used) == 3 && used == 1);
    CHECK(decode("\xc3\xa7", &used) == 0xe7 && used == 2);
    CHECK(decode("\xe2\x82\xac", &used) == 0x20ac && used == 3);
    CHECK(decode("\xf0\x9f\x98\x80", &used) == 0x1f600 && used == 4);
    CHECK(decode("\xc0\x80", &used) == PNL_KEY_NONE && used == 1);
    CHECK(decode("\xe0\x80\x80", &used) == PNL_KEY_NONE && used == 3);
    CHECK(decode("\xed\xa0\x80", &used) == PNL_KEY_NONE && used == 3);
    CHECK(decode("\xe2\x28", &used) == PNL_KEY_NONE && used == 1);
    CHECK(pnl_key_decode(NULL, 0, &key) == 0 && key == PNL_KEY_NONE);
    CHECK(pnl_key_decode((const unsigned char *)"a", 1, NULL) == 0);
}

static void test_build(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_on_status(app, noop_status, NULL) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 500) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 250) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 0) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 500) == PNL_OK);
    CHECK(pnl_app_add_action(app, "First", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Second", noop_action, NULL) == PNL_OK);
    pnl_app_destroy(app);
}

static void test_outside_callbacks(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_status_line(app, PNL_MARK_OK, NULL, "x") == PNL_ERR_ARG);
    CHECK(pnl_result(app, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    pnl_app_destroy(app);
}

static void test_no_terminal(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_add_action(app, "First", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_run(app, 0, NULL, NULL) == PNL_OK);
    pnl_app_destroy(app);
}

static void test_loop_parts(void)
{
    pnl_app_t *app = NULL;
    int first = 0;
    int second = 0;
    int calls = 0;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_add_timer(app, 100, noop_timer, NULL, &first) == PNL_OK);
    CHECK(pnl_app_add_timer(app, 200, NULL, NULL, &second) == PNL_OK);
    CHECK(first != second);
    CHECK(pnl_app_remove_timer(app, first) == PNL_OK);
    CHECK(pnl_app_remove_timer(app, first) == PNL_ERR_NOT_FOUND);
    CHECK(pnl_app_add_timer(app, 0, noop_timer, NULL, NULL) == PNL_ERR_ARG);
#if !defined(_WIN32)
    CHECK(pnl_app_watch_fd(app, 0, noop_fd, NULL) == PNL_OK);
    CHECK(pnl_app_watch_fd(app, 0, noop_fd, NULL) == PNL_OK);
    CHECK(pnl_app_unwatch_fd(app, 0) == PNL_OK);
    CHECK(pnl_app_unwatch_fd(app, 0) == PNL_ERR_NOT_FOUND);
#endif
    CHECK(pnl_view_set_hints(app, "x: y") == PNL_ERR_NOT_FOUND);
    CHECK(pnl_view_push(app, "live", noop_draw, NULL, NULL) == PNL_OK);
    CHECK(pnl_view_set_hints(app, "r: reload  esc: back") == PNL_OK);
    CHECK(pnl_view_set_hints(app, "r: reload") == PNL_OK);
    CHECK(pnl_view_set_hints(app, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push(app, NULL, noop_draw, NULL, NULL) == PNL_OK);
    CHECK(pnl_view_pop(app) == PNL_OK);
    CHECK(pnl_view_push_dialog(app, "ask", 30, 4, noop_draw, NULL, NULL) == PNL_OK);
    CHECK(pnl_view_pop(app) == PNL_OK);
    CHECK(pnl_view_push_dialog(app, "ask", 0, 4, noop_draw, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push_dialog(app, "ask", 30, 4, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_toast(app, PNL_MARK_OK, "one") == PNL_OK);
    CHECK(pnl_toast(app, PNL_MARK_WAIT, "two") == PNL_OK);
    CHECK(pnl_toast(app, PNL_MARK_FAIL, "three") == PNL_OK);
    CHECK(pnl_toast(app, PNL_MARK_OK, "four") == PNL_OK);
    CHECK(pnl_toast(app, 7, "bad") == PNL_ERR_ARG);
    CHECK(pnl_toast(app, PNL_MARK_OK, NULL) == PNL_ERR_ARG);
    CHECK(pnl_toast(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_app_with_terminal(app, count_call, &calls) == PNL_OK && calls == 1);
    CHECK(pnl_app_redraw(app) == PNL_OK);
    CHECK(pnl_app_quit(app) == PNL_OK);
    pnl_app_destroy(app);
}

static void write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");

    CHECK(file != NULL);
    fputs(text, file);
    fclose(file);
}

static void test_motion(void)
{
    pnl_app_t *app = NULL;
    long long now;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_motion(app) == 0);
    now = pnl_app_frame_ms(app);
    CHECK(now > 0);
    CHECK(pnl_app_animate(app, 500) == PNL_OK);
    CHECK(pnl_app_tween(app, now, 500) == 1.0);
    CHECK(pnl_app_tween(app, now, 0) == 1.0);
    CHECK(pnl_app_animate(app, -1) == PNL_ERR_ARG);
    CHECK(pnl_app_animate(NULL, 10) == PNL_ERR_ARG);
    CHECK(pnl_app_tween(NULL, now, 10) == 1.0);
    CHECK(pnl_app_motion(NULL) == 0);
    CHECK(pnl_app_frame_ms(NULL) == 0);
    CHECK(pnl_app_enable_motion(app, PNL_MOTION_SLIDE | PNL_MOTION_TRANSITION | PNL_MOTION_PULSE | PNL_MOTION_TYPEWRITER) == PNL_OK);
    CHECK(pnl_app_enable_motion(app, 0x4000) == PNL_ERR_ARG);
    CHECK(pnl_app_enable_motion(NULL, PNL_MOTION_SLIDE) == PNL_ERR_ARG);
    pnl_app_destroy(app);
}

static void test_layout(void)
{
    pnl_app_t *app = NULL;
    pnl_app_t *other = NULL;
    pnl_layout_t *layout = NULL;
    int tabs = -1;
    int panel = -1;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_create("other", NULL, NULL, NULL, &other) == PNL_OK);
    CHECK(pnl_layout_create(app, 7, &layout) == PNL_ERR_ARG && layout == NULL);
    CHECK(pnl_layout_create(NULL, PNL_SPLIT_ROWS, &layout) == PNL_ERR_ARG);
    CHECK(pnl_layout_create(app, PNL_SPLIT_ROWS, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_create(app, PNL_SPLIT_COLUMNS, &layout) == PNL_OK);
    CHECK(pnl_layout_split(layout, PNL_LAYOUT_ROOT, PNL_SPLIT_TABS, "tabs", &tabs) == PNL_OK && tabs > 0);
    CHECK(pnl_layout_panel(layout, tabs, "one", noop_draw, NULL, NULL, &panel) == PNL_OK && panel > tabs);
    CHECK(pnl_layout_panel(layout, panel, "inside a panel", noop_draw, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_split(layout, panel, PNL_SPLIT_ROWS, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_panel(layout, 99, "nowhere", noop_draw, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_panel(layout, tabs, "no draw", NULL, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_split(layout, PNL_LAYOUT_ROOT, 9, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_size(layout, panel, 10, 4) == PNL_OK);
    CHECK(pnl_layout_size(layout, PNL_LAYOUT_ROOT, 10, 4) == PNL_ERR_ARG);
    CHECK(pnl_layout_size(layout, panel, -1, 0) == PNL_ERR_ARG);
    CHECK(pnl_layout_focus(layout, panel) == PNL_OK);
    CHECK(pnl_layout_hints(layout, panel, "x: do it") == PNL_OK);
    CHECK(pnl_layout_hints(layout, panel, "y: again") == PNL_OK);
    CHECK(pnl_layout_hints(layout, tabs, "x: do it") == PNL_ERR_ARG);
    CHECK(pnl_layout_hints(layout, panel, NULL) == PNL_ERR_ARG);
    CHECK(pnl_layout_focus(layout, tabs) == PNL_ERR_ARG);
    CHECK(pnl_view_push_layout(other, "wrong app", layout) == PNL_ERR_ARG);
    CHECK(pnl_view_push_layout(app, "layout", NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push_layout(app, "layout", layout) == PNL_OK);
    CHECK(pnl_view_push_layout(app, NULL, layout) == PNL_OK);
    pnl_layout_destroy(layout);
    CHECK(pnl_view_pop(app) == PNL_OK);
    CHECK(pnl_view_pop(app) == PNL_OK);
    pnl_layout_destroy(NULL);
    CHECK(pnl_canvas_focused(NULL) == 0);
    pnl_app_destroy(other);
    pnl_app_destroy(app);
}

static void test_colors(void)
{
    const char *path = "tuikit_test_theme.txt";
    int depth = pnl_color_depth();
    pnl_app_t *app = NULL;

    CHECK(depth == 0 || depth == 16 || depth == 256 || depth == 16777216);
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_set_theme(app, "mono") == PNL_OK);
    CHECK(pnl_app_set_theme(app, "contrast") == PNL_OK);
    CHECK(pnl_app_set_theme(app, "nope") == PNL_ERR_NOT_FOUND);
    CHECK(pnl_app_set_theme(app, "./missing-theme.txt") == PNL_ERR_NOT_FOUND);
    write_file(path, "# mine\naccent = #ff00ff\nlight.accent = #880088\n\nmuted=#777777\n");
    CHECK(pnl_app_set_theme(app, path) == PNL_OK);
    write_file(path, "accent = magenta\n");
    CHECK(pnl_app_set_theme(app, path) == PNL_ERR_ARG);
    write_file(path, "border = #ffffff\n");
    CHECK(pnl_app_set_theme(app, path) == PNL_ERR_ARG);
    remove(path);
    CHECK(pnl_app_set_theme(app, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_set_theme(NULL, "mono") == PNL_ERR_ARG);
    pnl_app_destroy(app);
}

static void test_canvas_and_width(void)
{
    CHECK(pnl_text_width("abc") == 3);
    CHECK(pnl_text_width("\xc3\xa7" "a") == 2);
    CHECK(pnl_text_width("\xe4\xb8\xad\xe6\x96\x87") == 4);
    CHECK(pnl_text_width("e\xcc\x81") == 1);
    CHECK(pnl_text_width(NULL) == 0);
    CHECK(pnl_canvas_width(NULL) == 0);
    CHECK(pnl_canvas_height(NULL) == 0);
    CHECK(pnl_canvas_text(NULL, 1, 0, 0, "x") == 1);
    pnl_canvas_fill(NULL, 0, 0, 1, 1, 0);
}

static void fail_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_result(app, PNL_MARK_FAIL, "it \"failed\"\n");
}

static void view_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    CHECK(pnl_view_push(app, "live", noop_draw, NULL, NULL) == PNL_ERR_TTY);
}

static void wait_status(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_status_line(app, PNL_MARK_WAIT, "cache", "warming up");
    pnl_status_line(app, PNL_MARK_OK, NULL, "plain");
    pnl_status_meter(app, PNL_MARK_OK, "disk", 0.5);
}

static int then_calls;

static void count_then(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
    then_calls++;
}

static void counting_work(void *ctx, pnl_task_t *task)
{
    int *steps = ctx;

    CHECK(pnl_task_then(task, count_then, NULL) == PNL_OK);
    CHECK(pnl_task_then(task, NULL, NULL) == PNL_ERR_ARG);

    CHECK(pnl_task_step(task, "counting") == PNL_OK);
    for (*steps = 0; *steps < 3 && !pnl_task_cancelled(task); (*steps)++) {
        CHECK(pnl_task_log(task, "one more") == PNL_OK);
        pnl_task_progress(task, (unsigned long long)*steps + 1, 3);
    }
    CHECK(pnl_task_result(task, PNL_MARK_OK, "counted") == PNL_OK);
    CHECK(pnl_task_result(task, 7, "x") == PNL_ERR_ARG);
}

static int steps_counted;

static void task_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    CHECK(pnl_task_run(app, "count", counting_work, &steps_counted) == PNL_OK);
}

static int releases;

static const char *number_cell(void *ctx, size_t row, size_t column, char *buf, size_t buf_len)
{
    (void)ctx;
    if (column == 1)
        return NULL;
    snprintf(buf, buf_len, "%zu", row * 7 % 5);
    return buf;
}

static void count_release(void *ctx)
{
    (void)ctx;
    releases++;
}

static void components_action(void *ctx, pnl_app_t *app)
{
    static const char *const items[] = {"alpha", "beta"};
    static const char *const headers[] = {"name", "size"};
    static const int flags[] = {0, PNL_COLUMN_RIGHT | PNL_COLUMN_OPTIONAL};
    static const char *const cells[] = {"a", "10", "b", NULL};
    int *answers = ctx;
    char text[8] = "x";
    size_t index = 9;
    int ok = 1;

    CHECK(pnl_confirm(app, "sure?", &answers[0]) == PNL_OK);
    CHECK(pnl_input(app, "name", text, sizeof(text), &ok) == PNL_OK && !ok);
    CHECK(pnl_pick(app, "pick", items, 2, &index, &ok) == PNL_OK && !ok && index == 0);
    CHECK(pnl_pager(app, "text", "one\ttwo\nthree", 13) == PNL_OK);
    CHECK(pnl_pager(app, "empty", NULL, 0) == PNL_OK);
    CHECK(pnl_table(app, "table", headers, NULL, 2, cells, 2) == PNL_OK);
    CHECK(pnl_table(app, "table", headers, flags, 2, cells, 0) == PNL_OK);
    CHECK(pnl_pick(app, "pick", items, 0, &index, &ok) == PNL_ERR_ARG);
    CHECK(pnl_table(app, "table", headers, NULL, 0, cells, 1) == PNL_ERR_ARG);
    CHECK(pnl_table_source(app, "rows", headers, flags, 2, 3, number_cell, count_release, NULL) == PNL_OK);
    CHECK(pnl_table_source(app, "rows", headers, flags, 2, 3, NULL, count_release, NULL) == PNL_ERR_ARG);
    CHECK(pnl_table_source(app, "rows", headers, flags, 0, 3, number_cell, count_release, NULL) == PNL_ERR_ARG);
    CHECK(pnl_input(app, "name", text, 0, &ok) == PNL_ERR_ARG);
    answers[1] = 1;
}

static int run_cli(pnl_app_t *app, char *a, char *b)
{
    char *argv[] = {"demo", a, b, NULL};
    int argc = b ? 3 : a ? 2 : 1;
    int exit_code = -1;

    CHECK(pnl_app_run(app, argc, argv, &exit_code) == PNL_OK);
    return exit_code;
}

static void test_cli(void)
{
    int answers[2] = {0, 0};
    pnl_app_t *app = NULL;

#if defined(_WIN32)
    CHECK(freopen("NUL", "r", stdin) != NULL);
#else
    CHECK(freopen("/dev/null", "r", stdin) != NULL);
#endif

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_on_status(app, wait_status, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Do Nothing!", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Fail", fail_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Live view", view_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Count", task_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Components", components_action, answers) == PNL_OK);
    CHECK(pnl_task_run(app, "count", counting_work, &steps_counted) == PNL_ERR_ARG);
    CHECK(run_cli(app, "--help", NULL) == 0);
    CHECK(run_cli(app, "status", NULL) == 0);
    CHECK(run_cli(app, "status", "--json") == 0);
    CHECK(run_cli(app, "do-nothing", NULL) == 0);
    CHECK(run_cli(app, "fail", NULL) == 1);
    CHECK(run_cli(app, "fail", "--json") == 1);
    CHECK(run_cli(app, "live-view", NULL) == 1);
    CHECK(run_cli(app, "nope", NULL) == 2);
    CHECK(run_cli(app, "fail", "extra") == 2);
    CHECK(run_cli(app, "--bogus", NULL) == 2);
    CHECK(run_cli(app, "count", NULL) == 0 && steps_counted == 3 && then_calls == 1);
    CHECK(run_cli(app, "components", NULL) == 1 && answers[1] == 1 && answers[0] == 0 && releases == 1);
    CHECK(run_cli(app, "components", "--yes") == 0 && answers[0] == 1);
    pnl_app_destroy(app);
}

static void test_bad_args(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", failing_alloc, NULL, NULL, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create(NULL, NULL, NULL, NULL, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_on_status(NULL, noop_status, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(NULL, "x", noop_action, NULL) == PNL_ERR_ARG);
    CHECK(pnl_status_line(NULL, PNL_MARK_OK, NULL, "x") == PNL_ERR_ARG);
    CHECK(pnl_result(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_app_run(NULL, 0, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_quit(NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_redraw(NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_add_timer(NULL, 10, noop_timer, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_remove_timer(NULL, 1) == PNL_ERR_ARG);
    CHECK(pnl_app_watch_fd(NULL, 0, noop_fd, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_unwatch_fd(NULL, 0) == PNL_ERR_ARG);
    CHECK(pnl_app_with_terminal(NULL, count_call, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push(NULL, "x", noop_draw, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_pop(NULL) == PNL_ERR_ARG);
    CHECK(pnl_task_run(NULL, "x", counting_work, NULL) == PNL_ERR_ARG);
    CHECK(pnl_task_step(NULL, "x") == PNL_ERR_ARG);
    CHECK(pnl_task_log(NULL, "x") == PNL_ERR_ARG);
    CHECK(pnl_task_result(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_task_cancelled(NULL) == 0);
    pnl_task_progress(NULL, 1, 2);
    {
        int yes = 0;

        CHECK(pnl_confirm(NULL, "x", &yes) == PNL_ERR_ARG);
    }
    CHECK(pnl_pager(NULL, "x", "y", 1) == PNL_ERR_ARG);
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, -1) == PNL_ERR_ARG);
    CHECK(pnl_app_set_refresh(NULL, 100) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(app, NULL, noop_action, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(app, "x", NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push(app, "x", NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_pop(app) == PNL_ERR_NOT_FOUND);
    CHECK(pnl_app_with_terminal(app, NULL, NULL) == PNL_ERR_ARG);
    pnl_app_destroy(app);
    CHECK(pnl_strerror(PNL_ERR_TTY) != NULL);
    CHECK(pnl_strerror(-1) != NULL);
    pnl_app_destroy(NULL);
}

static int run_with_failure(failing_alloc_t *fa)
{
    pnl_app_t *app = NULL;
    int status;
    int i;

    status = pnl_app_create("demo", failing_alloc, failing_free, fa, &app);
    for (i = 0; i < 20 && status == PNL_OK; i++)
        status = pnl_app_add_action(app, "an action label", noop_action, NULL);
    for (i = 0; i < 20 && status == PNL_OK; i++)
        status = pnl_app_add_timer(app, 100, noop_timer, NULL, NULL);
    for (i = 0; i < 20 && status == PNL_OK; i++)
        status = pnl_view_push(app, "a view title", noop_draw, NULL, NULL);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, noop_status, NULL);
    if (status == PNL_OK)
        status = pnl_app_set_refresh(app, 500);
    pnl_app_destroy(app);
    return status;
}

static void test_alloc_failure(void)
{
    int n;

    for (n = 0;; n++) {
        failing_alloc_t fa = {n, 0};
        int status = run_with_failure(&fa);

        CHECK(fa.live == 0);
        if (status == PNL_OK)
            break;
        CHECK(status == PNL_ERR_NOMEM);
    }
    CHECK(n > 1);
}

static void test_values(void)
{
    pnl_app_t *app = NULL;
    pnl_value_t *first = NULL;
    pnl_value_t *second = NULL;
    char long_text[600];
    double zero = 0.0;

    memset(long_text, 'x', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_value_create(app, NULL, &first) == PNL_ERR_ARG && first == NULL);
    CHECK(pnl_value_create(NULL, "x", &first) == PNL_ERR_ARG);
    CHECK(pnl_value_create(app, "x", NULL) == PNL_ERR_ARG);
    CHECK(pnl_value_create(app, "queue", &first) == PNL_OK && first != NULL);
    CHECK(pnl_value_create(app, "disk", &second) == PNL_OK && second != NULL);
    CHECK(pnl_value_set(first, PNL_MARK_OK, "12 jobs") == PNL_OK);
    CHECK(pnl_value_set(first, PNL_MARK_OK, long_text) == PNL_OK);
    CHECK(pnl_value_set(first, 9, "x") == PNL_ERR_ARG);
    CHECK(pnl_value_set(first, PNL_MARK_OK, NULL) == PNL_ERR_ARG);
    CHECK(pnl_value_set(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_value_set_meter(second, PNL_MARK_WAIT, 1.5) == PNL_OK);
    CHECK(pnl_value_set_meter(second, PNL_MARK_WAIT, zero / zero) == PNL_ERR_ARG);
    CHECK(pnl_value_set_meter(NULL, PNL_MARK_OK, 0.5) == PNL_ERR_ARG);
    pnl_value_destroy(first);
    pnl_value_destroy(NULL);
    CHECK(pnl_value_create(app, "again", &first) == PNL_OK);
    pnl_value_destroy(second);
    pnl_value_destroy(first);
    pnl_app_destroy(app);
}

int main(void)
{
    test_keys();
    test_build();
    test_outside_callbacks();
    test_no_terminal();
    test_loop_parts();
    test_cli();
    test_colors();
    test_layout();
    test_values();
    test_motion();
    test_canvas_and_width();
    test_bad_args();
    test_alloc_failure();
    return 0;
}
