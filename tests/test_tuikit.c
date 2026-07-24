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
    CHECK(decode("\x1b[I", &used) > PNL_KEY_WHEEL_DOWN && used == 3);
    CHECK(decode("\x1b[O", &used) > PNL_KEY_WHEEL_DOWN && used == 3);
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

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
    CHECK(pnl_app_on_status(app, noop_status, NULL) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 500) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 250) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 0) == PNL_OK);
    CHECK(pnl_app_set_refresh(app, 500) == PNL_OK);
    CHECK(pnl_app_add_action(app, "First", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Second", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_set_memory(NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_bind(app, "ctrl-n", "down") == PNL_OK);
    CHECK(pnl_app_bind(app, "ctrl-n", "pgdn") == PNL_OK);
    CHECK(pnl_app_bind(app, "x", "q") == PNL_OK);
    CHECK(pnl_app_bind(app, "q", "none") == PNL_OK);
    CHECK(pnl_app_bind(app, "none", "q") == PNL_ERR_ARG);
    CHECK(pnl_app_bind(app, "ctrl-c", "q") == PNL_ERR_ARG);
    CHECK(pnl_app_bind(app, "hyper-x", "q") == PNL_ERR_ARG);
    CHECK(pnl_app_bind(app, "x", "") == PNL_ERR_ARG);
    CHECK(pnl_app_bind(NULL, "x", "q") == PNL_ERR_ARG);
    CHECK(pnl_log_create(app, 0, NULL) == PNL_ERR_ARG);
    CHECK(pnl_notify(app, "not running") == PNL_ERR_ARG);
    pnl_log_destroy(NULL);
    CHECK(pnl_app_set_memory(app, "") == PNL_ERR_ARG);
    CHECK(pnl_app_set_memory(app, "tuikit-memory") == PNL_OK);
    CHECK(pnl_app_set_memory(app, NULL) == PNL_OK);
    pnl_app_destroy(app);
}

static void test_outside_callbacks(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
    CHECK(pnl_status_line(app, PNL_MARK_OK, NULL, "x") == PNL_ERR_ARG);
    CHECK(pnl_result(app, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    pnl_app_destroy(app);
}

static void test_no_terminal(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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
    CHECK(pnl_view_shake(app) == PNL_ERR_NOT_FOUND);
    CHECK(pnl_view_shake(NULL) == PNL_ERR_ARG);
    CHECK(pnl_view_push(app, "live", noop_draw, NULL, NULL) == PNL_OK);
    CHECK(pnl_view_shake(app) == PNL_OK);
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

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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
    CHECK(pnl_app_enable_motion(app, PNL_MOTION_SLIDE | PNL_MOTION_TRANSITION | PNL_MOTION_PULSE | PNL_MOTION_TYPEWRITER |
                                         PNL_MOTION_ENTER | PNL_MOTION_FEEDBACK | PNL_MOTION_COUNT) == PNL_OK);
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

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
    CHECK(pnl_app_create("other", NULL, &other) == PNL_OK);
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
    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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
    pnl_log_t *events = NULL;
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
    CHECK(pnl_log_create(app, 2, &events) == PNL_OK);
    CHECK(pnl_log_write(events, PNL_LEVEL_INFO, "first") == PNL_OK);
    CHECK(pnl_log_write(events, PNL_LEVEL_WARN, "second\nignored") == PNL_OK);
    CHECK(pnl_log_write(events, PNL_LEVEL_ERROR, "third") == PNL_OK);
    CHECK(pnl_log_write(events, 9, "bad") == PNL_ERR_ARG);
    CHECK(pnl_log_write(events, PNL_LEVEL_INFO, NULL) == PNL_ERR_ARG);
    CHECK(pnl_log_open(app, events, "events") == PNL_OK);
    CHECK(pnl_log_open(app, NULL, "events") == PNL_ERR_ARG);
    CHECK(pnl_notify(app, "done") == PNL_OK);
    CHECK(pnl_notify(app, NULL) == PNL_ERR_ARG);
    pnl_log_destroy(events);
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

    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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

    pnl_allocator_t half = {failing_alloc, NULL, NULL};

    CHECK(pnl_app_create("demo", &half, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create(NULL, NULL, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create("demo", NULL, NULL) == PNL_ERR_ARG);
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
    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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

    pnl_allocator_t allocator;

    allocator.alloc_fn = failing_alloc;
    allocator.free_fn = failing_free;
    allocator.ctx = fa;
    status = pnl_app_create("demo", &allocator, &app);
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
    CHECK(pnl_app_create("demo", NULL, &app) == PNL_OK);
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

#ifndef PNL_SNAPSHOTS
#define PNL_SNAPSHOTS "tests/snapshots"
#endif

static int ticks;

static void tick_status(void *ctx, pnl_app_t *app)
{
    char text[32];

    (void)ctx;
    snprintf(text, sizeof(text), "%d ticks", ticks);
    pnl_status_line(app, PNL_MARK_OK, "clock", text);
}

static void tick(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
    ticks++;
}

static void ask_action(void *ctx, pnl_app_t *app)
{
    int yes = 0;

    (void)ctx;
    if (pnl_confirm(app, "really?", &yes) == PNL_OK)
        pnl_result(app, yes ? PNL_MARK_OK : PNL_MARK_FAIL, yes ? "confirmed" : "declined");
}

static void check_cast(const char *path)
{
    FILE *file = fopen(path, "rb");
    char line[4096];
    int events = 0;

    CHECK(file != NULL);
    CHECK(fgets(line, sizeof(line), file) != NULL);
    CHECK(strcmp(line, "{\"version\": 2, \"width\": 40, \"height\": 12, \"title\": \"screens\"}\n") == 0);
    while (fgets(line, sizeof(line), file) != NULL)
        events += line[0] == '[';
    fclose(file);
    remove(path);
    CHECK(events > 3);
}

static void test_screens(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    pnl_test_t *other = NULL;

    CHECK(pnl_app_create("screens", NULL, &app) == PNL_OK);
    CHECK(pnl_app_on_status(app, tick_status, NULL) == PNL_OK);
    CHECK(pnl_app_add_timer(app, 1000, tick, NULL, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Start", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Ask", ask_action, NULL) == PNL_OK);
    CHECK(pnl_app_record(app, "") == PNL_ERR_ARG);
    CHECK(pnl_app_record(app, "tuikit_test.cast") == PNL_OK);
    CHECK(pnl_test_create(app, 5, 5, &test) == PNL_ERR_ARG);
    CHECK(pnl_test_create(app, 40, 12, &test) == PNL_OK);
    CHECK(pnl_test_create(app, 40, 12, &other) == PNL_ERR_ARG && other == NULL);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/menu.txt") == PNL_OK);
    CHECK(pnl_test_wait(test, 1000) == PNL_OK && ticks == 1);
    CHECK(pnl_test_wait(test, 1000) == PNL_OK && ticks == 2);
    CHECK(strstr(pnl_test_screen(test), "2 ticks") != NULL);
    CHECK(pnl_test_keys(test, "down enter") == PNL_OK);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/confirm.txt") == PNL_OK);
    CHECK(pnl_test_keys(test, "y") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "confirmed") != NULL);
    CHECK(pnl_test_keys(test, "esc : \"sta\" enter") == PNL_OK);
    CHECK(pnl_test_keys(test, "hyper-x") == PNL_ERR_ARG);
    CHECK(pnl_test_keys(test, "esc q") == PNL_OK);
    CHECK(pnl_test_keys(test, "down") == PNL_OK);
    CHECK(pnl_test_expect(NULL, "x") == PNL_ERR_ARG);
    pnl_test_destroy(test);
    pnl_test_destroy(NULL);
    pnl_app_destroy(app);
    check_cast("tuikit_test.cast");
}

static void link_status(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_status_line(app, PNL_MARK_OK, "docs", "see https://x.test/a, then");
}

static void link_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    (void)ctx;
    (void)app;
    CHECK(pnl_canvas_link(canvas, 0, 0, 0, "manual", "https://x.test/manual") == 6);
    CHECK(pnl_canvas_link(canvas, 0, 1, 0, "x", NULL) == 0);
}

static void open_links(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_view_push(app, "links", link_draw, NULL, NULL);
}

static void test_links(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    FILE *file;
    char data[16384];
    size_t len;

    CHECK(pnl_app_create("links", NULL, &app) == PNL_OK);
    CHECK(pnl_app_on_status(app, link_status, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Open", open_links, NULL) == PNL_OK);
    CHECK(pnl_app_record(app, "tuikit_links.cast") == PNL_OK);
    CHECK(pnl_test_create(app, 40, 10, &test) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter") == PNL_OK);
    pnl_test_destroy(test);
    pnl_app_destroy(app);
    file = fopen("tuikit_links.cast", "rb");
    CHECK(file != NULL);
    len = fread(data, 1, sizeof(data) - 1, file);
    data[len] = '\0';
    fclose(file);
    remove("tuikit_links.cast");
    CHECK(strstr(data, "]8;;https://x.test/a\\u001b") != NULL);
    CHECK(strstr(data, "]8;;https://x.test/manual\\u001b") != NULL);
}

static void chart_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    static const double rising[] = {0.0, 12.5, 25.0, 50.0, 75.0, 100.0};
    static const double wave[] = {0.0, 100.0, 50.0, 0.0, 25.0, 75.0, 100.0};
    static const double pair[] = {10.0, 90.0};
    static const char *const names[] = {"in", "out"};

    (void)ctx;
    (void)app;
    pnl_canvas_chart(canvas, 0, 0, 6, 2, rising, 6, 100.0, 0);
    pnl_canvas_columns(canvas, 8, 0, 8, 3, pair, names, 2, 100.0, PNL_METER_LEVEL);
    pnl_canvas_plot(canvas, 18, 0, 4, 2, wave, 7, 0.0, 100.0, 0);
    pnl_canvas_bar(canvas, 0, 4, 10, 0.5, PNL_METER_PERCENT);
    pnl_canvas_plot(canvas, 0, 5, 4, 1000, wave, 7, 0.0, 100.0, 0);
    pnl_canvas_chart(canvas, 0, 5, 4, 2, rising, 6, 0.0, 0);
    CHECK(pnl_canvas_spinner(canvas, 0, 5, 0, PNL_SPINNER_DOTS) == PNL_OK);
    CHECK(pnl_canvas_spinner(canvas, 2, 5, 0, PNL_SPINNER_LINE) == PNL_OK);
    CHECK(pnl_canvas_spinner(canvas, 4, 5, 0, PNL_SPINNER_ARC) == PNL_OK);
    CHECK(pnl_canvas_spinner(canvas, 6, 5, 0, 99) == PNL_ERR_ARG);
    CHECK(pnl_canvas_spinner(NULL, 6, 5, 0, PNL_SPINNER_DOTS) == PNL_ERR_ARG);
    pnl_canvas_busy(canvas, 12, 5, 10, 0);
    pnl_canvas_busy(canvas, 12, 6, 2, 0);
}

static void open_charts(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_view_push(app, "charts", chart_draw, NULL, NULL);
}

static void test_charts(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;

    CHECK(pnl_app_create("charts", NULL, &app) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Open", open_charts, NULL) == PNL_OK);
    CHECK(pnl_test_create(app, 30, 10, &test) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter") == PNL_OK);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/charts.txt") == PNL_OK);
    pnl_test_destroy(test);
    pnl_app_destroy(app);
}

struct counter_widget {
    int count;
    int released;
};

static void counter_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct counter_widget *counter = ctx;
    char text[32];

    (void)app;
    snprintf(text, sizeof(text), "count %d", counter->count);
    pnl_canvas_text(canvas, 0, 0, 0, text);
}

static int counter_key(void *ctx, pnl_app_t *app, int key)
{
    struct counter_widget *counter = ctx;

    (void)app;
    if (key != '+')
        return 0;
    counter->count++;
    return 1;
}

static int counter_height(void *ctx, pnl_app_t *app, int width)
{
    (void)ctx;
    (void)app;
    (void)width;
    return 2;
}

static const char *counter_hints(void *ctx)
{
    (void)ctx;
    return "+: count";
}

static void counter_release(void *ctx)
{
    struct counter_widget *counter = ctx;

    counter->released++;
}

struct widget_screen {
    pnl_layout_t *layout;
    pnl_widget_t *stack;
};

static void open_widgets(void *ctx, pnl_app_t *app)
{
    struct widget_screen *screen = ctx;

    CHECK(pnl_view_push_layout(app, "widgets", screen->layout) == PNL_OK);
}

static void open_widget(void *ctx, pnl_app_t *app)
{
    struct widget_screen *screen = ctx;

    CHECK(pnl_view_push_widget(app, "one", screen->stack) == PNL_OK);
}

static void test_widgets(void)
{
    static const pnl_widget_class_t counter_class = {counter_draw, counter_key, counter_height, counter_hints,
                                                     counter_release};
    static const pnl_widget_class_t empty_class = {NULL, NULL, NULL, NULL, NULL};
    struct counter_widget counter = {0, 0};
    struct widget_screen screen = {NULL, NULL};
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    pnl_log_t *logbook = NULL;
    pnl_widget_t *custom = NULL;
    pnl_widget_t *text = NULL;
    pnl_widget_t *bar = NULL;
    pnl_widget_t *graph = NULL;
    pnl_widget_t *spinner = NULL;
    pnl_widget_t *log = NULL;
    pnl_widget_t *out = (pnl_widget_t *)&counter;
    int node = 0;

    CHECK(pnl_app_create("widgets", NULL, &app) == PNL_OK);
    CHECK(pnl_widget_create(app, &empty_class, NULL, &out) == PNL_ERR_ARG);
    CHECK(out == NULL);
    CHECK(pnl_widget_create(NULL, &counter_class, &counter, &custom) == PNL_ERR_ARG);
    CHECK(pnl_widget_create(app, &counter_class, &counter, &custom) == PNL_OK);
    CHECK(pnl_widget_ctx(custom) == &counter);
    CHECK(pnl_widget_ctx(NULL) == NULL);
    CHECK(pnl_widget_height(custom, 10) == 2);
    CHECK(pnl_widget_key(custom, '+') == 1);
    CHECK(pnl_widget_key(custom, 'x') == 0);
    CHECK(pnl_widget_key(NULL, '+') == 0);
    CHECK(pnl_widget_text(app, "first\nsecond", PNL_BOLD, &text) == PNL_OK);
    CHECK(pnl_widget_height(text, 10) == 2);
    CHECK(pnl_widget_bar(app, "fill", PNL_METER_PERCENT, &bar) == PNL_OK);
    CHECK(pnl_widget_set_value(bar, 0.5) == PNL_OK);
    CHECK(pnl_widget_graph(app, "load", PNL_GRAPH_CHART, 2, 0, 4, &graph) == PNL_OK);
    CHECK(pnl_widget_graph(app, "load", 7, 2, 0, 4, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_graph(app, "load", PNL_GRAPH_PLOT, 2, 0, 0, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_height(graph, 10) == 3);
    CHECK(pnl_widget_set_value(graph, 100.0) == PNL_OK);
    CHECK(pnl_widget_set_value(graph, 25.0) == PNL_OK);
    CHECK(pnl_widget_set_value(graph, 50.0) == PNL_OK);
    CHECK(pnl_widget_set_value(graph, 100.0) == PNL_OK);
    CHECK(pnl_widget_set_value(graph, 75.0) == PNL_OK);
    CHECK(pnl_widget_set_range(graph, 0.0, 100.0) == PNL_OK);
    CHECK(pnl_widget_set_range(graph, 2.0, 1.0) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_range(bar, 0.0, 1.0) == PNL_ERR_ARG);
    CHECK(pnl_widget_spinner(app, PNL_SPINNER_LINE, "waiting", &spinner) == PNL_OK);
    CHECK(pnl_widget_spinner(app, 9, "waiting", &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_text(spinner, "still waiting") == PNL_OK);
    CHECK(pnl_widget_set_text(custom, "x") == PNL_ERR_ARG);
    CHECK(pnl_widget_set_value(text, 1.0) == PNL_ERR_ARG);
    CHECK(pnl_log_create(app, 10, &logbook) == PNL_OK);
    CHECK(pnl_widget_log(app, logbook, &log) == PNL_OK);
    CHECK(pnl_widget_stack(app, 5, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_stack(app, PNL_STACK_ROWS, &screen.stack) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, screen.stack, 0) == PNL_ERR_ARG);
    CHECK(pnl_widget_add(text, custom, 0) == PNL_ERR_ARG);
    CHECK(pnl_widget_add(screen.stack, text, 0) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, custom, 0) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, bar, 0) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, spinner, 0) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, graph, 0) == PNL_OK);
    CHECK(pnl_widget_height(screen.stack, 20) == 9);
    CHECK(pnl_layout_create(app, PNL_SPLIT_COLUMNS, &screen.layout) == PNL_OK);
    CHECK(pnl_layout_widget(screen.layout, PNL_LAYOUT_ROOT, "parts", screen.stack, &node) == PNL_OK);
    CHECK(pnl_layout_widget(screen.layout, PNL_LAYOUT_ROOT, "log", log, NULL) == PNL_OK);
    CHECK(pnl_layout_widget(screen.layout, PNL_LAYOUT_ROOT, "none", NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(app, "Widgets", open_widgets, &screen) == PNL_OK);
    CHECK(pnl_app_add_action(app, "One", open_widget, &screen) == PNL_OK);
    CHECK(pnl_test_create(app, 60, 16, &test) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter + +") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "count 3") != NULL);
    CHECK(strstr(pnl_test_screen(test), "+: count  tab: next panel") != NULL);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/widgets.txt") == PNL_OK);
    CHECK(pnl_log_write(logbook, PNL_LEVEL_WARN, "disk almost full") == PNL_OK);
    CHECK(pnl_test_keys(test, "tab") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "disk almost") != NULL);
    CHECK(strstr(pnl_test_screen(test), "v: level") != NULL);
    CHECK(pnl_test_keys(test, "esc down enter +") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "count 4") != NULL);
    CHECK(strstr(pnl_test_screen(test), "+: count  esc: back") != NULL);
    pnl_test_destroy(test);
    pnl_layout_destroy(screen.layout);
    pnl_widget_destroy(screen.stack);
    pnl_widget_destroy(log);
    pnl_widget_destroy(spinner);
    pnl_widget_destroy(graph);
    pnl_widget_destroy(bar);
    pnl_widget_destroy(text);
    pnl_widget_destroy(custom);
    pnl_widget_destroy(NULL);
    CHECK(counter.released == 1);
    pnl_log_destroy(logbook);
    pnl_app_destroy(app);
}

struct form_screen {
    pnl_widget_t *stack;
    int pressed;
    int changes;
    pnl_widget_t *row;
    pnl_widget_t *cancel;
    int other;
};

static void form_other(void *ctx, pnl_app_t *app)
{
    struct form_screen *screen = ctx;

    (void)app;
    screen->other++;
}

static void open_form(void *ctx, pnl_app_t *app)
{
    struct form_screen *screen = ctx;

    CHECK(pnl_view_push_widget(app, "form", screen->stack) == PNL_OK);
}

static void form_pressed(void *ctx, pnl_app_t *app)
{
    struct form_screen *screen = ctx;

    (void)app;
    screen->pressed++;
}

static void form_changed(void *ctx, pnl_app_t *app, pnl_widget_t *widget)
{
    struct form_screen *screen = ctx;

    (void)app;
    (void)widget;
    screen->changes++;
}

static void test_forms(void)
{
    static const char *const colors[] = {"red", "green", "blue"};
    const char *const broken[] = {"one", NULL};
    struct form_screen screen = {NULL, 0, 0, NULL, NULL, 0};
    pnl_widget_t *fields[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    pnl_widget_t *out = NULL;
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    double value = -1.0;
    char long_text[300];
    size_t i;

    memset(long_text, 'a', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    CHECK(pnl_app_create("forms", NULL, &app) == PNL_OK);
    CHECK(pnl_widget_input(app, "name", 4, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_choice(app, "color", colors, 0, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_choice(app, "color", broken, 2, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_slider(app, "size", 5.0, 5.0, 1.0, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_button(app, NULL, form_pressed, &screen, &out) == PNL_ERR_ARG);
    CHECK(out == NULL);
    CHECK(pnl_widget_input(app, "name ", 0, &fields[0]) == PNL_OK);
    CHECK(pnl_widget_input(app, "pass ", PNL_INPUT_SECRET, &fields[1]) == PNL_OK);
    CHECK(pnl_widget_choice(app, "color", colors, 3, &fields[2]) == PNL_OK);
    CHECK(pnl_widget_slider(app, "size ", 0.0, 10.0, 2.0, &fields[3]) == PNL_OK);
    CHECK(pnl_widget_check(app, "loud", 0, &fields[4]) == PNL_OK);
    CHECK(pnl_widget_button(app, "Save", form_pressed, &screen, &fields[5]) == PNL_OK);
    CHECK(pnl_widget_stack(app, PNL_STACK_ROWS, &screen.stack) == PNL_OK);
    CHECK(pnl_widget_set_focusable(screen.stack, 1) == PNL_ERR_ARG);
    CHECK(pnl_widget_focus(screen.stack, fields[0]) == PNL_ERR_NOT_FOUND);
    CHECK(pnl_widget_button(app, "Cancel", form_other, &screen, &screen.cancel) == PNL_OK);
    CHECK(pnl_widget_stack(app, PNL_STACK_COLUMNS, &screen.row) == PNL_OK);
    for (i = 0; i < 6; i++) {
        CHECK(pnl_widget_add(i < 5 ? screen.stack : screen.row, fields[i], i < 5 ? 0 : 8) == PNL_OK);
        CHECK(pnl_widget_on_change(fields[i], form_changed, &screen) == PNL_OK);
    }
    CHECK(pnl_widget_add(screen.row, screen.cancel, 10) == PNL_OK);
    CHECK(pnl_widget_add(screen.stack, screen.row, 0) == PNL_OK);
    CHECK(pnl_widget_set_text(fields[0], long_text) == PNL_ERR_ARG);
    CHECK(pnl_widget_get_value(fields[0], &value) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_value(fields[2], 3.0) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_value(fields[3], 4.6) == PNL_OK);
    CHECK(pnl_widget_get_value(fields[3], &value) == PNL_OK && value == 4.0);
    CHECK(pnl_widget_set_value(fields[3], 0.0) == PNL_OK);
    CHECK(strcmp(pnl_widget_get_text(fields[2]), "red") == 0);
    CHECK(pnl_app_add_action(app, "Form", open_form, &screen) == PNL_OK);
    CHECK(pnl_test_create(app, 40, 12, &test) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter \"web q?\" tab \"pw\" tab right tab right right tab space tab enter") == PNL_OK);
    CHECK(strcmp(pnl_widget_get_text(fields[0]), "web q?") == 0);
    CHECK(strcmp(pnl_widget_get_text(fields[1]), "pw") == 0);
    CHECK(strcmp(pnl_widget_get_text(fields[2]), "green") == 0);
    CHECK(pnl_widget_get_value(fields[3], &value) == PNL_OK && value == 4.0);
    CHECK(pnl_widget_get_value(fields[4], &value) == PNL_OK && value == 1.0);
    CHECK(screen.pressed == 1);
    CHECK(screen.changes == 12);
    CHECK(strstr(pnl_test_screen(test), "enter: press  tab: next  esc: back") != NULL);
    CHECK(strstr(pnl_test_screen(test), "pw") == NULL);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/forms.txt") == PNL_OK);
    CHECK(pnl_test_keys(test, "tab tab \"x\" backtab left space") == PNL_OK);
    CHECK(strcmp(pnl_widget_get_text(fields[0]), "web q?x") == 0);
    CHECK(screen.pressed == 2);
    CHECK(pnl_widget_focus(screen.stack, fields[4]) == PNL_OK);
    CHECK(pnl_test_keys(test, "space down down") == PNL_OK);
    CHECK(pnl_widget_get_value(fields[4], &value) == PNL_OK && value == 0.0);
    CHECK(strstr(pnl_test_screen(test), "enter: press") != NULL);
    CHECK(pnl_test_keys(test, "right enter left enter") == PNL_OK);
    CHECK(screen.pressed == 3 && screen.other == 1);
    pnl_test_destroy(test);
    pnl_widget_destroy(screen.stack);
    pnl_widget_destroy(screen.row);
    pnl_widget_destroy(screen.cancel);
    for (i = 0; i < 6; i++)
        pnl_widget_destroy(fields[i]);
    pnl_app_destroy(app);
}

struct color_screen {
    int orange;
    int sunset;
};

static void draw_colors(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct color_screen *screen = ctx;

    (void)app;
    pnl_canvas_text(canvas, 0, 0, PNL_FG(screen->orange), "orange");
    pnl_canvas_gradient_text(canvas, 0, 1, PNL_BOLD, screen->sunset, "sunset");
    pnl_canvas_gradient_fill(canvas, 0, 2, 10, 1, screen->sunset);
    pnl_canvas_bar(canvas, 0, 3, 10, 0.5, PNL_METER_GRADIENT(screen->sunset) | PNL_METER_PERCENT);
}

static void open_colors(void *ctx, pnl_app_t *app)
{
    CHECK(pnl_view_push(app, "colors", draw_colors, NULL, ctx) == PNL_OK);
}

static void test_custom_colors(void)
{
    static const unsigned long stops[] = {0xff8800UL, 0xff0066UL, 0x6600ffUL};
    struct color_screen screen = {0, 0};
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    int color = -1;
    int again = -1;
    int gradient = -1;
    int count = 0;
    FILE *file;
    char cast[65536];
    size_t len;

    CHECK(pnl_app_create("colors", NULL, &app) == PNL_OK);
    CHECK(pnl_app_color(NULL, 0xff8800UL, &color) == PNL_ERR_ARG);
    CHECK(pnl_app_color(app, 0x1000000UL, &color) == PNL_ERR_ARG);
    CHECK(color == PNL_COLOR_DEFAULT);
    CHECK(pnl_app_color(app, 0xff8800UL, &screen.orange) == PNL_OK);
    CHECK(pnl_app_color(app, 0xff8800UL, &again) == PNL_OK);
    CHECK(again == screen.orange);
    CHECK(screen.orange > PNL_COLOR_BRIGHT_WHITE && screen.orange < 256);
    CHECK(pnl_app_gradient(app, stops, 1, 8, &gradient) == PNL_ERR_ARG);
    CHECK(pnl_app_gradient(app, stops, 3, 1, &gradient) == PNL_ERR_ARG);
    CHECK(pnl_app_gradient(app, stops, 3, 25, &gradient) == PNL_ERR_ARG);
    CHECK(gradient == 0);
    CHECK(pnl_app_gradient(app, stops, 3, 8, &screen.sunset) == PNL_OK);
    CHECK(pnl_gradient_color(screen.sunset, 0.0) + 7 == pnl_gradient_color(screen.sunset, 1.0));
    CHECK(pnl_gradient_color(screen.sunset, -1.0) == pnl_gradient_color(screen.sunset, 0.0));
    CHECK(pnl_gradient_color(screen.sunset, 0.5) != screen.orange);
    while (pnl_app_color(app, 0x010101UL * (unsigned long)(count + 1), &color) == PNL_OK)
        count++;
    CHECK(count > 30 && count < 60);
    CHECK(pnl_app_gradient(app, stops, 3, 24, &gradient) == PNL_ERR_NOMEM);
    CHECK(pnl_app_add_action(app, "Colors", open_colors, &screen) == PNL_OK);
    CHECK(pnl_app_record(app, "colors.cast") == PNL_OK);
    CHECK(pnl_test_create(app, 30, 8, &test) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "sunset") != NULL);
    CHECK(strstr(pnl_test_screen(test), "50%") != NULL);
    pnl_test_destroy(test);
    pnl_app_destroy(app);
    file = fopen("colors.cast", "rb");
    CHECK(file != NULL);
    if (file == NULL)
        return;
    len = fread(cast, 1, sizeof(cast) - 1, file);
    cast[len] = '\0';
    fclose(file);
    remove("colors.cast");
    CHECK(strstr(cast, "38;2;255;136;0") != NULL);
    CHECK(strstr(cast, "48;2;102;0;255") != NULL);
}

static void open_big(void *ctx, pnl_app_t *app)
{
    CHECK(pnl_view_push_widget(app, "big", ctx) == PNL_OK);
}

static void test_big_text(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    pnl_widget_t *stack = NULL;
    pnl_widget_t *big = NULL;
    pnl_widget_t *art = NULL;
    pnl_widget_t *out = NULL;

    CHECK(pnl_big_width(PNL_BIG_SMALL, "12:3") == 13);
    CHECK(pnl_big_width(PNL_BIG_LARGE, "12:3") == 26);
    CHECK(pnl_big_width(PNL_BIG_SMALL, "~") == 3);
    CHECK(pnl_big_width(7, "1") == 0 && pnl_big_width(PNL_BIG_SMALL, NULL) == 0);
    CHECK(pnl_big_height(PNL_BIG_SMALL) == 3 && pnl_big_height(PNL_BIG_LARGE) == 5);
    CHECK(pnl_app_create("ship", NULL, &app) == PNL_OK);
    CHECK(pnl_app_set_banner(NULL, NULL, 0) == PNL_ERR_ARG);
    CHECK(pnl_app_set_banner(app, NULL, 0) == PNL_OK);
    CHECK(pnl_widget_big(app, "42%", 3, 0, 0, &out) == PNL_ERR_ARG);
    CHECK(pnl_widget_big(app, NULL, PNL_BIG_SMALL, 0, 0, &out) == PNL_ERR_ARG);
    CHECK(out == NULL);
    CHECK(pnl_widget_big(app, "42%", PNL_BIG_LARGE, PNL_FG(PNL_ROLE_OK), 0, &big) == PNL_OK);
    CHECK(pnl_widget_art(app, "/\\\n\\/", 0, 0, &art) == PNL_OK);
    CHECK(pnl_widget_height(big, 20) == 5 && pnl_widget_height(art, 20) == 2);
    CHECK(pnl_widget_set_text(big, "7:05") == PNL_OK);
    CHECK(strcmp(pnl_widget_get_text(big), "7:05") == 0);
    CHECK(pnl_widget_stack(app, PNL_STACK_ROWS, &stack) == PNL_OK);
    CHECK(pnl_widget_add(stack, big, 0) == PNL_OK);
    CHECK(pnl_widget_add(stack, art, 0) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Big", open_big, stack) == PNL_OK);
    CHECK(pnl_test_create(app, 40, 14, &test) == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "\xe2\x96\x80") != NULL);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/banner.txt") == PNL_OK);
    CHECK(pnl_test_keys(test, "enter") == PNL_OK);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/big.txt") == PNL_OK);
    CHECK(pnl_test_keys(test, "esc") == PNL_OK);
    CHECK(pnl_app_set_banner(app, "+--+\n|ok|\n+--+", 0) == PNL_OK);
    CHECK(pnl_test_wait(test, 10) == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "|ok|") != NULL);
    pnl_test_destroy(test);
    CHECK(pnl_test_create(app, 40, 8, &test) == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "|ok|") == NULL);
    pnl_test_destroy(test);
    pnl_widget_destroy(stack);
    pnl_widget_destroy(big);
    pnl_widget_destroy(art);
    pnl_app_destroy(app);
}

static void open_live(void *ctx, pnl_app_t *app)
{
    CHECK(pnl_view_push_widget(app, "live", ctx) == PNL_OK);
}

static void test_live_art(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    pnl_widget_t *flow = NULL;
    pnl_widget_t *scene = NULL;
    pnl_widget_t *stage = NULL;
    double state = -1;
    int step = -1;

    CHECK(pnl_app_create("ship", NULL, &app) == PNL_OK);
    CHECK(pnl_widget_flow(app, &flow) == PNL_OK);
    CHECK(pnl_widget_add_step(flow, "build", &step) == PNL_OK && step == 0);
    CHECK(pnl_widget_add_step(flow, "test", NULL) == PNL_OK);
    CHECK(pnl_widget_add_step(flow, "ship", &step) == PNL_OK && step == 2);
    CHECK(pnl_widget_add_step(NULL, "x", NULL) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_step(flow, 3, PNL_STEP_OK, NULL) == PNL_ERR_NOT_FOUND);
    CHECK(pnl_widget_set_step(flow, 0, 9, NULL) == PNL_ERR_ARG);
    CHECK(pnl_widget_set_step(flow, 0, PNL_STEP_OK, "1.2s") == PNL_OK);
    CHECK(pnl_widget_set_step(flow, 1, PNL_STEP_FAIL, "2 failed") == PNL_OK);
    CHECK(pnl_widget_set_step(flow, 2, PNL_STEP_SKIP, NULL) == PNL_OK);
    CHECK(pnl_widget_height(flow, 60) == 4 && pnl_widget_height(flow, 20) == 5);
    CHECK(pnl_widget_scene(app, &scene) == PNL_OK);
    CHECK(pnl_widget_add_scene(scene, 1, "a\fb", 0, 0, 100) == PNL_OK);
    CHECK(pnl_widget_add_scene(scene, 2, "\\o/\nok", 0, 0, 0) == PNL_OK);
    CHECK(pnl_widget_add_scene(scene, 3, "x", 0, 0, 0) == PNL_OK);
    CHECK(pnl_widget_add_scene(scene, 3, "no\nluck", 0, 0, 0) == PNL_OK);
    CHECK(pnl_widget_add_scene(flow, 1, "x", 0, 0, 0) == PNL_ERR_ARG);
    CHECK(pnl_widget_height(scene, 20) == 2);
    CHECK(pnl_widget_set_value(scene, 3) == PNL_OK);
    CHECK(pnl_widget_get_value(scene, &state) == PNL_OK && state == 3);
    CHECK(pnl_widget_stack(app, PNL_STACK_ROWS, &stage) == PNL_OK);
    CHECK(pnl_widget_add(stage, flow, 0) == PNL_OK);
    CHECK(pnl_widget_add(stage, scene, 0) == PNL_OK);
    CHECK(pnl_app_set_banner_widget(app, flow) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Live", open_live, stage) == PNL_OK);
    CHECK(pnl_test_create(app, 50, 16, &test) == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "build") != NULL && strstr(pnl_test_screen(test), "> 1 Live") != NULL);
    CHECK(pnl_test_keys(test, "enter") == PNL_OK);
    CHECK(pnl_test_expect(test, PNL_SNAPSHOTS "/live.txt") == PNL_OK);
    CHECK(pnl_widget_set_value(scene, 2) == PNL_OK);
    CHECK(pnl_test_wait(test, 10) == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "\\o/") != NULL && strstr(pnl_test_screen(test), "luck") == NULL);
    pnl_test_destroy(test);
    CHECK(pnl_app_set_banner_widget(app, NULL) == PNL_OK);
    CHECK(pnl_app_set_banner_widget(NULL, flow) == PNL_ERR_ARG);
    CHECK(pnl_task_show(NULL, flow) == PNL_ERR_ARG);
    pnl_widget_destroy(stage);
    pnl_widget_destroy(flow);
    pnl_widget_destroy(scene);
    pnl_app_destroy(app);
}

static void test_inspector(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;

    CHECK(pnl_app_create("inspect", NULL, &app) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Ask", ask_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_timer(app, 1000, tick, NULL, NULL) == PNL_OK);
    CHECK(pnl_test_create(app, 100, 24, &test) == PNL_OK);
    CHECK(pnl_test_wait(test, 1000) == PNL_OK);
    CHECK(pnl_test_keys(test, "enter f12") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "drawn") != NULL);
    CHECK(strstr(pnl_test_screen(test), "menu > confirm (dialog)") != NULL);
    CHECK(strstr(pnl_test_screen(test), "1 actions") != NULL);
    CHECK(strstr(pnl_test_screen(test), "1 timers") != NULL);
    CHECK(strstr(pnl_test_screen(test), "key f12") != NULL);
    CHECK(strstr(pnl_test_screen(test), "key enter") != NULL);
    CHECK(strstr(pnl_test_screen(test), "timer 1") != NULL);
    CHECK(pnl_test_keys(test, "f12") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "really?") != NULL);
    CHECK(pnl_test_keys(test, "f12 q") == PNL_OK);
    CHECK(strstr(pnl_test_screen(test), "really?") != NULL);
    pnl_test_destroy(test);
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
    test_screens();
    test_links();
    test_inspector();
    test_charts();
    test_widgets();
    test_forms();
    test_custom_colors();
    test_big_text();
    test_live_art();
    return 0;
}
