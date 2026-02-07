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
#define PNL_STATUS_TIMER_NONE 0
#define PNL_TASK_TICK_MS 50

#define PNL_STYLE_ATTRS(style) ((uint32_t)(style) & 0xffu)
#define PNL_STYLE_FG(style) (((uint32_t)(style) >> 8) & 0xffu)
#define PNL_STYLE_BG(style) (((uint32_t)(style) >> 16) & 0xffu)

struct pnl_mem {
    pnl_alloc_fn alloc_fn;
    pnl_free_fn free_fn;
    void *ctx;
};

struct pnl_cell {
    uint32_t cp;
    uint32_t style;
};

struct pnl_screen {
    int width;
    int height;
    int front_valid;
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
    char *text;
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
};

struct pnl_app {
    struct pnl_mem mem;
    char *title;
    struct pnl_action *actions;
    size_t actions_len;
    size_t actions_cap;
    pnl_status_fn status_fn;
    void *status_ctx;
    int status_timer;
    struct pnl_line *lines;
    size_t lines_len;
    size_t lines_cap;
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
    int running;
    int cli;
    int cli_json;
    int cli_yes;
    int cli_color;
    int cli_worst;
    int cli_needs_tty;
    int quit;
    int dirty;
    struct pnl_term term;
    struct pnl_screen screen;
};

PNL_INTERNAL void *pnl_mem_alloc(const struct pnl_mem *mem, size_t size);
PNL_INTERNAL void pnl_mem_free(const struct pnl_mem *mem, void *ptr);
PNL_INTERNAL char *pnl_mem_copy(const struct pnl_mem *mem, const char *text);
PNL_INTERNAL int pnl_mem_grow(const struct pnl_mem *mem, void **items, size_t *cap, size_t needed, size_t item_size);

PNL_INTERNAL size_t pnl_utf8_next(const char *text, size_t text_len, uint32_t *out_cp);
PNL_INTERNAL int pnl_cp_width(uint32_t cp);

PNL_INTERNAL int pnl_screen_resize(struct pnl_screen *screen, const struct pnl_mem *mem, int width, int height);
PNL_INTERNAL void pnl_screen_free(struct pnl_screen *screen, const struct pnl_mem *mem);
PNL_INTERNAL void pnl_screen_clear(struct pnl_screen *screen);
PNL_INTERNAL int pnl_screen_text(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text);
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
PNL_INTERNAL int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key);
PNL_INTERNAL int pnl_term_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len,
                               int timeout_ms, unsigned char *out_ready);
PNL_INTERNAL long long pnl_term_now_ms(void);

PNL_INTERNAL int pnl_thread_start(struct pnl_thread *thread, void (*fn)(void *arg), void *arg);
PNL_INTERNAL void pnl_thread_join(struct pnl_thread *thread);
PNL_INTERNAL int pnl_lock_init(struct pnl_lock *lock);
PNL_INTERNAL void pnl_lock_destroy(struct pnl_lock *lock);
PNL_INTERNAL void pnl_lock(struct pnl_lock *lock);
PNL_INTERNAL void pnl_unlock(struct pnl_lock *lock);

PNL_INTERNAL void pnl_edit_init(struct pnl_edit *edit, char *text, size_t cap);
PNL_INTERNAL int pnl_edit_key(struct pnl_edit *edit, int key);
PNL_INTERNAL void pnl_edit_draw(const struct pnl_edit *edit, pnl_canvas_t *canvas, int x, int y, int width, int focused);
PNL_INTERNAL int pnl_canvas_clip(pnl_canvas_t *canvas, int x, int y, int max, int style, const char *text);
PNL_INTERNAL int pnl_cli_table(pnl_app_t *app, const char *const *headers, size_t columns, const char *const *cells, size_t rows);
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
PNL_INTERNAL int pnl_run_interactive(pnl_app_t *app);
PNL_INTERNAL int pnl_cli_result(pnl_app_t *app, int mark, const char *text);
PNL_INTERNAL void pnl_cli_progress(const char *step, unsigned long long done, unsigned long long total);
PNL_INTERNAL int pnl_cli_read_line(const char *prompt, char *text, size_t text_len);
PNL_INTERNAL void pnl_cli_log(const pnl_app_t *app, const char *line);
PNL_INTERNAL void pnl_draw_mark(pnl_canvas_t *canvas, int y, int mark, const char *text);
PNL_INTERNAL int pnl_view_push_internal(pnl_app_t *app, const struct pnl_view *view);
PNL_INTERNAL int pnl_loop(pnl_app_t *app, size_t min_views);
PNL_INTERNAL int pnl_view_modal(pnl_app_t *app, struct pnl_view *view);
PNL_INTERNAL void pnl_views_drop(pnl_app_t *app);
PNL_INTERNAL void pnl_timers_run(pnl_app_t *app);
PNL_INTERNAL int pnl_timers_timeout(const pnl_app_t *app);

#endif
