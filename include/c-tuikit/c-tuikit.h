#ifndef C_TUIKIT_H
#define C_TUIKIT_H

#include <stddef.h>

#if defined(PNL_STATIC) && defined(__GNUC__)
#define PNL_API static __attribute__((unused))
#elif defined(PNL_STATIC)
#define PNL_API static
#elif defined(_WIN32) && defined(PNL_BUILD_SHARED)
#define PNL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(PNL_SHARED)
#define PNL_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(PNL_BUILD_SHARED)
#define PNL_API __attribute__((visibility("default")))
#else
#define PNL_API
#endif

#if defined(PNL_NO_DEPRECATED_WARNINGS)
#define PNL_DEPRECATED(msg)
#elif defined(__GNUC__)
#define PNL_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define PNL_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define PNL_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PNL_OK 0
#define PNL_ERR_ARG 1
#define PNL_ERR_NOMEM 2
#define PNL_ERR_TTY 3
#define PNL_ERR_IO 4
#define PNL_ERR_NOT_FOUND 5
#define PNL_ERR_UNSUPPORTED 6
#define PNL_ERR_MISMATCH 7

#define PNL_MARK_OK 0
#define PNL_MARK_WAIT 1
#define PNL_MARK_FAIL 2

#define PNL_LEVEL_DEBUG 0
#define PNL_LEVEL_INFO 1
#define PNL_LEVEL_WARN 2
#define PNL_LEVEL_ERROR 3

#define PNL_KEY_NONE 0
#define PNL_KEY_UP 0x110000
#define PNL_KEY_DOWN 0x110001
#define PNL_KEY_LEFT 0x110002
#define PNL_KEY_RIGHT 0x110003
#define PNL_KEY_ENTER 0x110004
#define PNL_KEY_ESCAPE 0x110005
#define PNL_KEY_PAGE_UP 0x110006
#define PNL_KEY_PAGE_DOWN 0x110007
#define PNL_KEY_HOME 0x110008
#define PNL_KEY_END 0x110009
#define PNL_KEY_BACKSPACE 0x11000a
#define PNL_KEY_DELETE 0x11000b
#define PNL_KEY_TAB 0x11000c
#define PNL_KEY_BACKTAB 0x11000d
#define PNL_KEY_CLICK 0x11000e
#define PNL_KEY_WHEEL_UP 0x11000f
#define PNL_KEY_WHEEL_DOWN 0x110010

#define PNL_BOLD 0x01
#define PNL_DIM 0x02
#define PNL_REVERSE 0x04
#define PNL_UNDERLINE 0x08
#define PNL_FG(color) ((color) << 8)
#define PNL_BG(color) ((color) << 16)

#define PNL_COLOR_DEFAULT 0
#define PNL_COLOR_BLACK 1
#define PNL_COLOR_RED 2
#define PNL_COLOR_GREEN 3
#define PNL_COLOR_YELLOW 4
#define PNL_COLOR_BLUE 5
#define PNL_COLOR_MAGENTA 6
#define PNL_COLOR_CYAN 7
#define PNL_COLOR_WHITE 8
#define PNL_COLOR_GRAY 9
#define PNL_COLOR_BRIGHT_RED 10
#define PNL_COLOR_BRIGHT_GREEN 11
#define PNL_COLOR_BRIGHT_YELLOW 12
#define PNL_COLOR_BRIGHT_BLUE 13
#define PNL_COLOR_BRIGHT_MAGENTA 14
#define PNL_COLOR_BRIGHT_CYAN 15
#define PNL_COLOR_BRIGHT_WHITE 16

#define PNL_ROLE_ACCENT 32
#define PNL_ROLE_OK 33
#define PNL_ROLE_WARN 34
#define PNL_ROLE_FAIL 35
#define PNL_ROLE_MUTED 36
#define PNL_ROLE_INFO 37
#define PNL_ROLE_SURFACE 38

#define PNL_MOTION_SLIDE 1
#define PNL_MOTION_TRANSITION 2
#define PNL_MOTION_PULSE 4
#define PNL_MOTION_TYPEWRITER 8
#define PNL_MOTION_ENTER 16
#define PNL_MOTION_FEEDBACK 32
#define PNL_MOTION_COUNT 64

#define PNL_COLUMN_RIGHT 1
#define PNL_COLUMN_OPTIONAL 2

#define PNL_SPLIT_ROWS 0
#define PNL_SPLIT_COLUMNS 1
#define PNL_SPLIT_TABS 2
#define PNL_LAYOUT_ROOT 0

#define PNL_METER_ACCENT 0
#define PNL_METER_LEVEL 1
#define PNL_METER_PERCENT 2

#define PNL_SPINNER_DOTS 0
#define PNL_SPINNER_LINE 1
#define PNL_SPINNER_ARC 2
#define PNL_SPINNER_BLOCKS 3
#define PNL_SPINNER_BOUNCE 4

#define PNL_GRAPH_SPARKLINE 0
#define PNL_GRAPH_CHART 1
#define PNL_GRAPH_PLOT 2

#define PNL_STACK_ROWS 0
#define PNL_STACK_COLUMNS 1
#define PNL_INPUT_SECRET 1

typedef struct pnl_app pnl_app_t;
typedef struct pnl_canvas pnl_canvas_t;
typedef struct pnl_task pnl_task_t;
typedef struct pnl_layout pnl_layout_t;
typedef struct pnl_value pnl_value_t;
typedef struct pnl_log pnl_log_t;
typedef struct pnl_test pnl_test_t;
typedef struct pnl_widget pnl_widget_t;

typedef void *(*pnl_alloc_fn)(void *ctx, size_t size);
typedef void (*pnl_free_fn)(void *ctx, void *ptr);

typedef struct pnl_allocator {
    pnl_alloc_fn alloc_fn;
    pnl_free_fn free_fn;
    void *ctx;
} pnl_allocator_t;

typedef void (*pnl_status_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_action_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_timer_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_fd_fn)(void *ctx, pnl_app_t *app, int fd);
typedef void (*pnl_draw_fn)(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas);
typedef int (*pnl_key_fn)(void *ctx, pnl_app_t *app, int key);
typedef void (*pnl_terminal_fn)(void *ctx);
typedef void (*pnl_work_fn)(void *ctx, pnl_task_t *task);
typedef const char *(*pnl_cell_fn)(void *ctx, size_t row, size_t column, char *buf, size_t buf_len);
typedef void (*pnl_release_fn)(void *ctx);
typedef int (*pnl_height_fn)(void *ctx, pnl_app_t *app, int width);
typedef const char *(*pnl_hints_fn)(void *ctx);
typedef void (*pnl_change_fn)(void *ctx, pnl_app_t *app, pnl_widget_t *widget);

typedef struct pnl_widget_class {
    pnl_draw_fn draw_fn;
    pnl_key_fn key_fn;
    pnl_height_fn height_fn;
    pnl_hints_fn hints_fn;
    pnl_release_fn release_fn;
} pnl_widget_class_t;

PNL_API int pnl_app_create(const char *title, const pnl_allocator_t *allocator, pnl_app_t **out_app);
PNL_API void pnl_app_destroy(pnl_app_t *app);
PNL_API int pnl_app_on_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx);
PNL_API int pnl_app_set_refresh(pnl_app_t *app, int refresh_ms);
PNL_API int pnl_app_set_theme(pnl_app_t *app, const char *theme);
PNL_API int pnl_app_set_memory(pnl_app_t *app, const char *path);
PNL_API int pnl_app_bind(pnl_app_t *app, const char *key, const char *to);
PNL_API int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx);
PNL_API int pnl_status_line(pnl_app_t *app, int mark, const char *label, const char *text);
PNL_API int pnl_mouse_position(const pnl_app_t *app, int *out_x, int *out_y);
PNL_API int pnl_notify(pnl_app_t *app, const char *text);
PNL_API int pnl_app_record(pnl_app_t *app, const char *path);
PNL_API int pnl_test_create(pnl_app_t *app, int width, int height, pnl_test_t **out_test);
PNL_API void pnl_test_destroy(pnl_test_t *test);
PNL_API int pnl_test_keys(pnl_test_t *test, const char *keys);
PNL_API int pnl_test_wait(pnl_test_t *test, int ms);
PNL_API const char *pnl_test_screen(const pnl_test_t *test);
PNL_API int pnl_test_expect(pnl_test_t *test, const char *path);
PNL_API int pnl_toast(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_status_meter(pnl_app_t *app, int mark, const char *label, double fraction);

PNL_API int pnl_value_create(pnl_app_t *app, const char *label, pnl_value_t **out_value);
PNL_API void pnl_value_destroy(pnl_value_t *value);
PNL_API int pnl_value_set(pnl_value_t *value, int mark, const char *text);
PNL_API int pnl_value_set_meter(pnl_value_t *value, int mark, double fraction);
PNL_API int pnl_log_create(pnl_app_t *app, size_t capacity, pnl_log_t **out_log);
PNL_API void pnl_log_destroy(pnl_log_t *logbook);
PNL_API int pnl_log_write(pnl_log_t *logbook, int level, const char *text);
PNL_API int pnl_log_open(pnl_app_t *app, pnl_log_t *logbook, const char *title);
PNL_API int pnl_result(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_app_run(pnl_app_t *app, int argc, char **argv, int *out_exit_code);
PNL_API int pnl_app_quit(pnl_app_t *app);
PNL_API int pnl_app_redraw(pnl_app_t *app);
PNL_API int pnl_app_motion(const pnl_app_t *app);
PNL_API int pnl_app_enable_motion(pnl_app_t *app, int flags);
PNL_API long long pnl_app_frame_ms(const pnl_app_t *app);
PNL_API int pnl_app_animate(pnl_app_t *app, int duration_ms);
PNL_API double pnl_app_tween(pnl_app_t *app, long long start_ms, int duration_ms);
PNL_API int pnl_app_add_timer(pnl_app_t *app, int interval_ms, pnl_timer_fn timer_fn, void *timer_ctx, int *out_timer);
PNL_API int pnl_app_remove_timer(pnl_app_t *app, int timer);
PNL_API int pnl_app_watch_fd(pnl_app_t *app, int fd, pnl_fd_fn fd_fn, void *fd_ctx);
PNL_API int pnl_app_unwatch_fd(pnl_app_t *app, int fd);
PNL_API int pnl_app_with_terminal(pnl_app_t *app, pnl_terminal_fn terminal_fn, void *terminal_ctx);
PNL_API int pnl_view_push(pnl_app_t *app, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *view_ctx);
PNL_API int pnl_view_push_dialog(pnl_app_t *app, const char *title, int width, int height, pnl_draw_fn draw_fn,
                                 pnl_key_fn key_fn, void *view_ctx);
PNL_API int pnl_view_pop(pnl_app_t *app);
PNL_API int pnl_view_shake(pnl_app_t *app);
PNL_API int pnl_view_set_hints(pnl_app_t *app, const char *hints);
PNL_API int pnl_task_run(pnl_app_t *app, const char *title, pnl_work_fn work_fn, void *work_ctx);
PNL_API void pnl_task_progress(pnl_task_t *task, unsigned long long done, unsigned long long total);
PNL_API int pnl_task_step(pnl_task_t *task, const char *text);
PNL_API int pnl_task_log(pnl_task_t *task, const char *line);
PNL_API int pnl_task_cancelled(pnl_task_t *task);
PNL_API int pnl_task_result(pnl_task_t *task, int mark, const char *text);
PNL_API int pnl_task_then(pnl_task_t *task, pnl_action_fn then_fn, void *then_ctx);
PNL_API int pnl_confirm(pnl_app_t *app, const char *question, int *out_yes);
PNL_API int pnl_input(pnl_app_t *app, const char *prompt, char *text, size_t text_len, int *out_ok);
PNL_API int pnl_pick(pnl_app_t *app, const char *title, const char *const *items, size_t items_len,
                     size_t *out_index, int *out_ok);
PNL_API int pnl_pager(pnl_app_t *app, const char *title, const char *text, size_t text_len);
PNL_API int pnl_table_source(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags,
                             size_t columns, size_t rows, pnl_cell_fn cell_fn, pnl_release_fn release_fn, void *ctx);
PNL_API int pnl_table(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags,
                      size_t columns, const char *const *cells, size_t rows);
PNL_API int pnl_layout_create(pnl_app_t *app, int kind, pnl_layout_t **out_layout);
PNL_API void pnl_layout_destroy(pnl_layout_t *layout);
PNL_API int pnl_layout_split(pnl_layout_t *layout, int parent, int kind, const char *title, int *out_node);
PNL_API int pnl_layout_panel(pnl_layout_t *layout, int parent, const char *title, pnl_draw_fn draw_fn,
                             pnl_key_fn key_fn, void *panel_ctx, int *out_node);
PNL_API int pnl_layout_size(pnl_layout_t *layout, int node, int size, int min_size);
PNL_API int pnl_layout_focus(pnl_layout_t *layout, int node);
PNL_API int pnl_layout_hints(pnl_layout_t *layout, int node, const char *hints);
PNL_API int pnl_view_push_layout(pnl_app_t *app, const char *title, const pnl_layout_t *layout);
PNL_API int pnl_layout_widget(pnl_layout_t *layout, int parent, const char *title, pnl_widget_t *widget,
                              int *out_node);

PNL_API int pnl_widget_create(pnl_app_t *app, const pnl_widget_class_t *widget_class, void *widget_ctx,
                              pnl_widget_t **out_widget);
PNL_API void pnl_widget_destroy(pnl_widget_t *widget);
PNL_API void *pnl_widget_ctx(const pnl_widget_t *widget);
PNL_API int pnl_widget_draw(pnl_widget_t *widget, pnl_canvas_t *canvas, int x, int y, int width, int height);
PNL_API int pnl_widget_key(pnl_widget_t *widget, int key);
PNL_API int pnl_widget_height(pnl_widget_t *widget, int width);
PNL_API int pnl_view_push_widget(pnl_app_t *app, const char *title, pnl_widget_t *widget);
PNL_API int pnl_widget_text(pnl_app_t *app, const char *text, int style, pnl_widget_t **out_widget);
PNL_API int pnl_widget_bar(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget);
PNL_API int pnl_widget_graph(pnl_app_t *app, const char *label, int kind, int rows, int flags, size_t capacity,
                             pnl_widget_t **out_widget);
PNL_API int pnl_widget_spinner(pnl_app_t *app, int kind, const char *text, pnl_widget_t **out_widget);
PNL_API int pnl_widget_log(pnl_app_t *app, pnl_log_t *logbook, pnl_widget_t **out_widget);
PNL_API int pnl_widget_stack(pnl_app_t *app, int kind, pnl_widget_t **out_widget);
PNL_API int pnl_widget_add(pnl_widget_t *stack, pnl_widget_t *child, int size);
PNL_API int pnl_widget_set_text(pnl_widget_t *widget, const char *text);
PNL_API int pnl_widget_set_value(pnl_widget_t *widget, double value);
PNL_API int pnl_widget_set_range(pnl_widget_t *widget, double min, double max);
PNL_API int pnl_widget_input(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget);
PNL_API int pnl_widget_check(pnl_app_t *app, const char *label, int checked, pnl_widget_t **out_widget);
PNL_API int pnl_widget_choice(pnl_app_t *app, const char *label, const char *const *options, size_t count,
                              pnl_widget_t **out_widget);
PNL_API int pnl_widget_slider(pnl_app_t *app, const char *label, double min, double max, double step,
                              pnl_widget_t **out_widget);
PNL_API int pnl_widget_button(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx,
                              pnl_widget_t **out_widget);
PNL_API int pnl_widget_on_change(pnl_widget_t *widget, pnl_change_fn change_fn, void *change_ctx);
PNL_API const char *pnl_widget_get_text(const pnl_widget_t *widget);
PNL_API int pnl_widget_get_value(const pnl_widget_t *widget, double *out_value);
PNL_API int pnl_widget_set_focusable(pnl_widget_t *widget, int focusable);
PNL_API int pnl_widget_focus(pnl_widget_t *stack, pnl_widget_t *child);

PNL_API int pnl_canvas_focused(const pnl_canvas_t *canvas);
PNL_API int pnl_canvas_width(const pnl_canvas_t *canvas);
PNL_API int pnl_canvas_height(const pnl_canvas_t *canvas);
PNL_API int pnl_canvas_text(pnl_canvas_t *canvas, int x, int y, int style, const char *text);
PNL_API int pnl_canvas_link(pnl_canvas_t *canvas, int x, int y, int style, const char *text, const char *url);
PNL_API void pnl_canvas_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int style);
PNL_API int pnl_color_depth(void);
PNL_API void pnl_canvas_bar(pnl_canvas_t *canvas, int x, int y, int width, double fraction, int flags);
PNL_API void pnl_canvas_sparkline(pnl_canvas_t *canvas, int x, int y, int width, const double *values,
                                  size_t values_len, double max, int flags);
PNL_API int pnl_canvas_spinner(pnl_canvas_t *canvas, int x, int y, int style, int kind);
PNL_API void pnl_canvas_busy(pnl_canvas_t *canvas, int x, int y, int width, int flags);
PNL_API void pnl_canvas_chart(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                              size_t values_len, double max, int flags);
PNL_API void pnl_canvas_columns(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                                const char *const *labels, size_t count, double max, int flags);
PNL_API void pnl_canvas_plot(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                             size_t values_len, double min, double max, int flags);
PNL_API int pnl_text_width(const char *text);
PNL_API size_t pnl_key_decode(const unsigned char *data, size_t data_len, int *out_key);
PNL_API const char *pnl_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif
