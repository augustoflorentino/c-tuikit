#ifndef PNL_INTERNAL_H
#define PNL_INTERNAL_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "c-tuikit/c-tuikit.h"

#include <stdint.h>

#if defined(PNL_IMPLEMENTATION_DONE) && defined(__GNUC__)
#define PNL_INTERNAL static __attribute__((unused))
#elif defined(PNL_IMPLEMENTATION_DONE)
#define PNL_INTERNAL static
#else
#define PNL_INTERNAL
#endif

#define PNL_INPUT_MAX 64
#define PNL_WATCH_MAX 64
#define PNL_ESCAPE_WAIT_MS 50
#define PNL_KEY_SUSPEND 0x1a
#define PNL_KEY_CTRL_C 3
#define PNL_KEY_FOCUS_IN 0x110011
#define PNL_KEY_FOCUS_OUT 0x110012
#define PNL_KEY_INSPECT 0x110013
#define PNL_EVENT_KEY 0
#define PNL_EVENT_WAKE 1
#define PNL_EVENT_TIMER 2
#define PNL_EVENT_WATCH 3
#define PNL_EVENT_RESIZE 4
#define PNL_STATUS_TIMER_NONE 0
#define PNL_TASK_TICK_MS 50
#define PNL_FRAME_MS 33
#define PNL_SPIN_MS 80
#define PNL_BUSY_MS 1600
#define PNL_GLIDE_MS 140
#define PNL_MOTION_ALL                                                                                          \
    (PNL_MOTION_SLIDE | PNL_MOTION_TRANSITION | PNL_MOTION_PULSE | PNL_MOTION_TYPEWRITER | PNL_MOTION_ENTER | \
     PNL_MOTION_FEEDBACK | PNL_MOTION_COUNT)
#define PNL_ENTER_MS 150
#define PNL_EXIT_MS 120
#define PNL_CASCADE_STEP_MS 30
#define PNL_CASCADE_STEPS 8
#define PNL_SHAKE_MS 240
#define PNL_COUNT_MS 450
#define PNL_SHIMMER_MS 1200
#define PNL_PULSE_MS 1200
#define PNL_PULSE_DEPTH 7
#define PNL_TYPE_CHAR_MS 12
#define PNL_TYPE_MAX_MS 400
#define PNL_SLOW_LINK_MS 60
#define PNL_TRANSITION_MS 160
#define PNL_LEVEL_FIRST 64
#define PNL_LEVEL_STEPS 64
#define PNL_FADE_FIRST 128
#define PNL_FADE_STEPS 16
#define PNL_FADE_MS 600
#define PNL_GRADIENT_STEPS_MAX 24
#define PNL_DEPTH_NONE 0
#define PNL_DEPTH_16 1
#define PNL_DEPTH_256 2
#define PNL_DEPTH_TRUE 3

#define PNL_STYLE_ATTRS(style) ((uint32_t)(style) & 0xffu)
#define PNL_STYLE_FG(style) (((uint32_t)(style) >> 8) & 0xffu)
#define PNL_STYLE_BG(style) (((uint32_t)(style) >> 16) & 0xffu)
#define PNL_STYLE_LINK(style) (((uint32_t)(style) >> 24) & 0xffu)
#define PNL_LINKS 255
#define PNL_LINK_POOL 8192
#define PNL_PLOT_ROWS 256
#define PNL_WIDGET_FOOTER 256
#define PNL_WIDGET_TEXT 1
#define PNL_WIDGET_BAR 2
#define PNL_WIDGET_GRAPH 3
#define PNL_WIDGET_SPINNER 4
#define PNL_WIDGET_STACK 5
#define PNL_WIDGET_INPUT 6
#define PNL_WIDGET_CHECK 7
#define PNL_WIDGET_CHOICE 8
#define PNL_WIDGET_SLIDER 9
#define PNL_WIDGET_BUTTON 10
#define PNL_WIDGET_BIG 11
#define PNL_WIDGET_ART 12
#define PNL_WIDGET_FLOW 13
#define PNL_WIDGET_SCENE 14

struct pnl_big_widget {
    pnl_app_t *app;
    char *text;
    int size;
    int style;
    int gradient;
};

struct pnl_widget_ops {
    int (*set_text)(pnl_widget_t *widget, const char *text);
    const char *(*get_text)(const pnl_widget_t *widget);
    int (*set_value)(pnl_widget_t *widget, double value);
    int (*get_value)(const pnl_widget_t *widget, double *out_value);
    int (*set_range)(pnl_widget_t *widget, double min, double max);
};

struct pnl_widget {
    pnl_app_t *app;
    pnl_widget_class_t widget_class;
    const struct pnl_widget_ops *ops;
    void *ctx;
    int kind;
    int focusable;
    pnl_change_fn change_fn;
    void *change_ctx;
    char footer_text[PNL_WIDGET_FOOTER];
    const char *footer;
};

struct pnl_mem {
    pnl_alloc_fn alloc_fn;
    pnl_free_fn free_fn;
    void *ctx;
};

struct pnl_cell {
    uint32_t cp;
    uint32_t style;
};

#define PNL_ROLES (PNL_ROLE_SURFACE - PNL_ROLE_ACCENT + 1)

struct pnl_palette {
    int depth;
    int light;
    unsigned long background;
    unsigned long rgb[PNL_ROLES];
    int x256[PNL_ROLES];
    int basic[PNL_ROLES];
    unsigned long custom[256];
    unsigned char custom_used[256];
};

struct pnl_theme {
    unsigned long dark[PNL_ROLES];
    unsigned long light[PNL_ROLES];
    int basic[PNL_ROLES];
};

struct pnl_screen {
    int width;
    int height;
    int front_valid;
    struct pnl_palette palette;
    int spin;
    int pulse;
    int spinning;
    int linear;
    int links_off;
    int links_len;
    size_t link_pool_len;
    unsigned long links_hash;
    unsigned long links_drawn;
    unsigned short *link_at;
    char *link_pool;
    struct pnl_cell *back;
    struct pnl_cell *front;
    char *out;
    size_t out_len;
    size_t out_cap;
};

struct pnl_term {
    int active;
    unsigned int surrogate;
    unsigned char input[PNL_INPUT_MAX];
    size_t input_len;
    int mouse;
    int linear;
    int mouse_x;
    int mouse_y;
    int focus_known;
    int focused;
    int wake_open;
    int wake_fd[2];
    void *wake_event;
    int woken;
};

union pnl_storage {
    long long align_int;
    double align_float;
    void *align_ptr;
    unsigned char bytes[128];
};

struct pnl_thread {
    void (*fn)(void *arg);
    void *arg;
    void *handle;
    union pnl_storage storage;
};

struct pnl_lock {
    union pnl_storage storage;
};

struct pnl_edit {
    char *text;
    size_t cap;
    size_t len;
    size_t cursor;
};

struct pnl_canvas {
    struct pnl_screen *screen;
    int focused;
    int x;
    int y;
    int width;
    int height;
};

struct pnl_action {
    char *label;
    pnl_action_fn fn;
    void *ctx;
};

struct pnl_line {
    int mark;
    double meter;
    char *label;
    char *text;
};

struct pnl_glide {
    double from;
    double to;
    long long start_ms;
};

#define PNL_BINDINGS 64

struct pnl_binding {
    int from;
    int to;
};

struct pnl_memo {
    char *key;
    char *value;
};

struct pnl_seen {
    unsigned long id;
    unsigned long hash;
    long long changed_ms;
    long long count_ms;
    unsigned long count_prefix;
    double count_from;
    double count_to;
    long long meter_ms;
    double meter_from;
    double meter_to;
};

struct pnl_timer {
    int id;
    int interval_ms;
    long long due_ms;
    pnl_timer_fn fn;
    void *ctx;
};

struct pnl_watch {
    int fd;
    pnl_fd_fn fn;
    void *ctx;
};

typedef void (*pnl_drop_fn)(pnl_app_t *app, void *ctx);

struct pnl_view {
    char *title;
    char *hints;
    const char *footer;
    const char *const *footer_ref;
    pnl_draw_fn draw_fn;
    pnl_key_fn key_fn;
    pnl_drop_fn drop_fn;
    void *ctx;
    const int *typing_ref;
    int dialog_width;
    int dialog_height;
    int origin_x;
    int origin_y;
    int box_x;
    int box_y;
    int box_width;
    int box_height;
    long long shown_ms;
};

struct pnl_ghost {
    long long ms;
    int x;
    int y;
    int width;
    int height;
};

#define PNL_CELL_MAX 256
#define PNL_TOASTS 3
#define PNL_TOAST_TEXT 120

struct pnl_toast {
    int mark;
    long long shown_ms;
    char text[PNL_TOAST_TEXT];
};

struct pnl_inspect;

struct pnl_app {
    struct pnl_mem mem;
    char *title;
    char *banner;
    int banner_gradient;
    int banner_set;
    pnl_widget_t *banner_widget;
    struct pnl_action *actions;
    size_t actions_len;
    size_t actions_cap;
    pnl_status_fn status_fn;
    void *status_ctx;
    int status_timer;
    struct pnl_line *lines;
    size_t lines_len;
    size_t lines_cap;
    struct pnl_seen *seen;
    size_t seen_len;
    size_t seen_cap;
    struct pnl_seen *seen_next;
    size_t seen_next_cap;
    int seen_ready;
    int in_status;
    size_t selected;
    size_t menu_top;
    struct pnl_view *views;
    size_t views_len;
    size_t views_cap;
    struct pnl_timer *timers;
    size_t timers_len;
    size_t timers_cap;
    int next_timer;
    struct pnl_watch *watches;
    size_t watches_len;
    size_t watches_cap;
    int tasks_running;
    int motion;
    int motion_flags;
    struct pnl_toast toasts[PNL_TOASTS];
    struct pnl_binding bindings[PNL_BINDINGS];
    size_t bindings_len;
    char *memory_path;
    char *record_path;
    struct pnl_inspect *inspect;
    struct pnl_memo *memos;
    size_t memos_len;
    size_t memos_cap;
    int memos_dirty;
    pnl_value_t **values;
    size_t values_len;
    size_t values_cap;
    int mouse_ox;
    int mouse_oy;
    int menu_y;
    int menu_end;
    size_t toasts_len;
    long reply_ms;
    struct pnl_glide select_glide;
    long long transition_ms;
    long long cascade_ms;
    long long shake_ms;
    long long chosen_ms;
    size_t chosen;
    struct pnl_ghost ghost;
    int transition_shift;
    int anim_frame;
    long long frame_ms;
    long long animate_until_ms;
    int running;
    int cli;
    int cli_json;
    int cli_yes;
    int cli_color;
    int cli_worst;
    int cli_needs_tty;
    int quit;
    int dirty;
    int widget_typing;
    struct pnl_term term;
    struct pnl_screen screen;
    struct pnl_theme theme;
};

PNL_INTERNAL void *pnl_mem_alloc(const struct pnl_mem *mem, size_t size);
PNL_INTERNAL void pnl_mem_free(const struct pnl_mem *mem, void *ptr);
PNL_INTERNAL char *pnl_mem_copy(const struct pnl_mem *mem, const char *text);
PNL_INTERNAL int pnl_mem_grow(const struct pnl_mem *mem, void **items, size_t *cap, size_t needed, size_t item_size);

PNL_INTERNAL size_t pnl_utf8_put(uint32_t cp, char *out);
PNL_INTERNAL size_t pnl_utf8_next(const char *text, size_t text_len, uint32_t *out_cp);
PNL_INTERNAL int pnl_cp_width(uint32_t cp);

PNL_INTERNAL int pnl_screen_resize(struct pnl_screen *screen, const struct pnl_mem *mem, int width, int height);
PNL_INTERNAL void pnl_screen_free(struct pnl_screen *screen, const struct pnl_mem *mem);
PNL_INTERNAL void pnl_screen_clear(struct pnl_screen *screen);
PNL_INTERNAL int pnl_screen_text(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text);
PNL_INTERNAL int pnl_screen_link(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text,
                                 const char *url);
PNL_INTERNAL void pnl_screen_fill(struct pnl_screen *screen, int x, int y, int width, uint32_t style);
PNL_INTERNAL int pnl_screen_render(struct pnl_screen *screen, const struct pnl_mem *mem);

PNL_INTERNAL int pnl_term_available(void);
PNL_INTERNAL int pnl_term_stdout_is_tty(void);
PNL_INTERNAL int pnl_term_stderr_is_tty(void);
PNL_INTERNAL int pnl_term_stdin_is_tty(void);
PNL_INTERNAL int pnl_term_enter(struct pnl_term *term);
PNL_INTERNAL void pnl_term_leave(struct pnl_term *term);
PNL_INTERNAL void pnl_term_suspend(struct pnl_term *term);
PNL_INTERNAL int pnl_term_resized(void);
PNL_INTERNAL void pnl_term_size(int *out_width, int *out_height);
PNL_INTERNAL int pnl_term_write(const char *data, size_t data_len);
PNL_INTERNAL int pnl_term_fill(struct pnl_term *term, int timeout_ms);
PNL_INTERNAL int pnl_term_wake_open(struct pnl_term *term);
PNL_INTERNAL void pnl_term_wake_close(struct pnl_term *term);
PNL_INTERNAL void pnl_term_wake(struct pnl_term *term);
PNL_INTERNAL void pnl_values_lines(pnl_app_t *app);
PNL_INTERNAL int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key);
PNL_INTERNAL int pnl_term_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len,
                               int timeout_ms, unsigned char *out_ready);
PNL_INTERNAL long long pnl_term_now_ms(void);
PNL_INTERNAL int pnl_os_enter(struct pnl_term *term);
PNL_INTERNAL void pnl_os_leave(struct pnl_term *term);
PNL_INTERNAL void pnl_os_suspend(struct pnl_term *term);
PNL_INTERNAL int pnl_os_resized(void);
PNL_INTERNAL void pnl_os_size(int *out_width, int *out_height);
PNL_INTERNAL int pnl_os_write(const char *data, size_t data_len);
PNL_INTERNAL void pnl_os_wake(struct pnl_term *term);
PNL_INTERNAL int pnl_os_read_key(struct pnl_term *term, int timeout_ms, int *out_key);
PNL_INTERNAL int pnl_os_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len,
                             int timeout_ms, unsigned char *out_ready);
PNL_INTERNAL long long pnl_os_now_ms(void);
PNL_INTERNAL int pnl_virtual_active(void);
PNL_INTERNAL int pnl_env_off(const char *name);
PNL_INTERNAL long long pnl_clock_us(void);
PNL_INTERNAL void pnl_inspect_start(pnl_app_t *app);
PNL_INTERNAL void pnl_inspect_free(pnl_app_t *app);
PNL_INTERNAL void pnl_inspect_frame(pnl_app_t *app, long long took_us, size_t bytes);
PNL_INTERNAL void pnl_inspect_event(pnl_app_t *app, int kind, int arg);
PNL_INTERNAL void pnl_inspect_toggle(pnl_app_t *app);
PNL_INTERNAL void pnl_widget_paint(pnl_widget_t *widget, pnl_canvas_t *canvas);
PNL_INTERNAL const char *pnl_widget_hints(const pnl_widget_t *widget);
PNL_INTERNAL int pnl_widget_focusable(const pnl_widget_t *widget);
PNL_INTERNAL int pnl_widget_builtin(pnl_app_t *app, int kind, const pnl_widget_class_t *widget_class,
                                     const struct pnl_widget_ops *ops, void *ctx, pnl_widget_t **out_widget);
PNL_INTERNAL int pnl_widget_copy(pnl_app_t *app, char **slot, const char *text);
PNL_INTERNAL int pnl_banner_draw(pnl_app_t *app, pnl_canvas_t *canvas, int needed);
PNL_INTERNAL void pnl_widget_focus_edge(pnl_widget_t *widget, int last);
PNL_INTERNAL void pnl_widget_changed(pnl_widget_t *widget);
PNL_INTERNAL int pnl_art_size(const char *art, int *out_width);
PNL_INTERNAL int pnl_theme_line(struct pnl_theme *theme, char *line, int *light_set);
PNL_INTERNAL void pnl_memory_parse(pnl_app_t *app, char *data);
PNL_INTERNAL void pnl_keymap_line(pnl_app_t *app, char *line);
PNL_INTERNAL void pnl_edit_draw_styled(const struct pnl_edit *edit, pnl_canvas_t *canvas, int x, int y, int width,
                                       int focused, int style);
PNL_INTERNAL void pnl_record_open(pnl_app_t *app);
PNL_INTERNAL void pnl_record_output(const char *data, size_t data_len, int width, int height);
PNL_INTERNAL void pnl_record_close(void);
PNL_INTERNAL void pnl_nap_ms(int ms);

PNL_INTERNAL int pnl_thread_start(struct pnl_thread *thread, void (*fn)(void *arg), void *arg);
PNL_INTERNAL void pnl_thread_join(struct pnl_thread *thread);
PNL_INTERNAL int pnl_lock_init(struct pnl_lock *lock);
PNL_INTERNAL void pnl_lock_destroy(struct pnl_lock *lock);
PNL_INTERNAL void pnl_lock(struct pnl_lock *lock);
PNL_INTERNAL void pnl_unlock(struct pnl_lock *lock);

PNL_INTERNAL int pnl_color_detect(void);
PNL_INTERNAL int pnl_color_ink(char *buf, unsigned ink, int background, const struct pnl_palette *palette);
PNL_INTERNAL int pnl_is_light(long background_rgb);
PNL_INTERNAL void pnl_palette_init(struct pnl_palette *palette, const struct pnl_theme *theme, int depth, int light,
                                   long background_rgb, int high_contrast);
PNL_INTERNAL int pnl_rgb_to_256(unsigned long rgb);
PNL_INTERNAL int pnl_theme_choose(struct pnl_theme *theme, const char *name);
PNL_INTERNAL int pnl_background_detect(struct pnl_term *term, int depth, long *out_rgb, long *out_reply_ms);
PNL_INTERNAL long pnl_link_probe(struct pnl_term *term);

PNL_INTERNAL void pnl_tool_name(char *out, size_t out_len, const char *title);
PNL_INTERNAL int pnl_key_parse(const char *name, int *out_key);
PNL_INTERNAL void pnl_key_label(int key, char *out, size_t out_len);
PNL_INTERNAL int pnl_keymap_apply(const pnl_app_t *app, int key);
PNL_INTERNAL void pnl_keymap_load(pnl_app_t *app);
PNL_INTERNAL const char *pnl_memory_get(const pnl_app_t *app, const char *kind, const char *name);
PNL_INTERNAL void pnl_memory_set(pnl_app_t *app, const char *kind, const char *name, const char *value);
PNL_INTERNAL void pnl_memory_load(pnl_app_t *app);
PNL_INTERNAL void pnl_memory_save(pnl_app_t *app);
PNL_INTERNAL void pnl_memory_free(pnl_app_t *app);
PNL_INTERNAL void pnl_edit_init(struct pnl_edit *edit, char *text, size_t cap);
PNL_INTERNAL int pnl_edit_key(struct pnl_edit *edit, int key);
PNL_INTERNAL void pnl_edit_draw(const struct pnl_edit *edit, pnl_canvas_t *canvas, int x, int y, int width, int focused);
PNL_INTERNAL int pnl_level_color(double fraction);
PNL_INTERNAL int pnl_fade_style(pnl_app_t *app, const pnl_canvas_t *canvas, int role, long long start_ms, int from_step);
PNL_INTERNAL int pnl_motion_on(const pnl_app_t *app, int flag);
PNL_INTERNAL double pnl_glide_at(pnl_app_t *app, const struct pnl_glide *glide, int flag);
PNL_INTERNAL void pnl_glide_to(pnl_app_t *app, struct pnl_glide *glide, double target, int flag);
PNL_INTERNAL void pnl_dialog_draw(pnl_app_t *app, const pnl_canvas_t *area, struct pnl_view *view, int focused);
PNL_INTERNAL int pnl_menu_mouse(pnl_app_t *app, int key);
PNL_INTERNAL int pnl_label_width(const pnl_app_t *app);
PNL_INTERNAL void pnl_wheel_keys(pnl_app_t *app, pnl_key_fn key_fn, void *ctx, int key);
PNL_INTERNAL int pnl_is_mouse_key(int key);
PNL_INTERNAL long long pnl_toasts_next_ms(const pnl_app_t *app);
PNL_INTERNAL void pnl_toasts_expire(pnl_app_t *app);
PNL_INTERNAL void pnl_toasts_draw(pnl_app_t *app);
PNL_INTERNAL void pnl_ghost_draw(pnl_app_t *app);
PNL_INTERNAL double pnl_ease_out(double t);
PNL_INTERNAL int pnl_shake_offset(pnl_app_t *app);
PNL_INTERNAL void pnl_draw_spinner(pnl_canvas_t *canvas, int x, int y, int style);
PNL_INTERNAL int pnl_text_width_n(const char *text, size_t len);
PNL_INTERNAL int pnl_canvas_clip(pnl_canvas_t *canvas, int x, int y, int max, int style, const char *text);
PNL_INTERNAL int pnl_cli_table(pnl_app_t *app, const char *const *headers, const int *column_flags, size_t columns,
                               size_t rows, pnl_cell_fn cell_fn, void *ctx);
PNL_INTERNAL int pnl_text_contains_at(const char *text, const char *needle);
PNL_INTERNAL int pnl_text_contains(const char *text, const char *needle);

PNL_INTERNAL void pnl_menu_draw(pnl_app_t *app, pnl_canvas_t *canvas);
PNL_INTERNAL void pnl_help(pnl_app_t *app);
PNL_INTERNAL const char *pnl_view_hints(const pnl_app_t *app);
PNL_INTERNAL void pnl_palette(pnl_app_t *app);
PNL_INTERNAL int pnl_menu_key(pnl_app_t *app, int key);
PNL_INTERNAL void pnl_menu_clear_lines(pnl_app_t *app);
PNL_INTERNAL void pnl_menu_refresh(pnl_app_t *app);
PNL_INTERNAL int pnl_valid_mark(int mark);
PNL_INTERNAL int pnl_key_partial(const unsigned char *data, size_t data_len);
PNL_INTERNAL int pnl_term_complete(struct pnl_term *term);
PNL_INTERNAL size_t pnl_key_decode_at(const unsigned char *data, size_t data_len, int *out_key, int *out_x, int *out_y);
PNL_INTERNAL int pnl_run_interactive(pnl_app_t *app);
PNL_INTERNAL int pnl_cli_result(pnl_app_t *app, int mark, const char *text);
PNL_INTERNAL void pnl_cli_progress(const char *step, unsigned long long done, unsigned long long total);
PNL_INTERNAL int pnl_cli_read_line(const char *prompt, char *text, size_t text_len);
PNL_INTERNAL void pnl_task_notify(pnl_app_t *app, const char *title, long long started_ms, const char *outcome);
PNL_INTERNAL void pnl_json_string(const char *text);
PNL_INTERNAL void pnl_cli_log(const pnl_app_t *app, const char *line);
PNL_INTERNAL void pnl_draw_mark(pnl_canvas_t *canvas, int y, int mark, const char *text);
PNL_INTERNAL int pnl_view_push_internal(pnl_app_t *app, const struct pnl_view *view);
PNL_INTERNAL int pnl_loop(pnl_app_t *app, size_t min_views);
PNL_INTERNAL int pnl_view_modal(pnl_app_t *app, struct pnl_view *view);
PNL_INTERNAL void pnl_views_drop(pnl_app_t *app);
PNL_INTERNAL void pnl_timers_run(pnl_app_t *app);
PNL_INTERNAL int pnl_timers_timeout(const pnl_app_t *app);

#endif
