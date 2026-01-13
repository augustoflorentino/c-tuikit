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

#define PNL_MARK_OK 0
#define PNL_MARK_WAIT 1
#define PNL_MARK_FAIL 2

#define PNL_KEY_NONE 0
#define PNL_KEY_UP 0x110000
#define PNL_KEY_DOWN 0x110001
#define PNL_KEY_LEFT 0x110002
#define PNL_KEY_RIGHT 0x110003
#define PNL_KEY_ENTER 0x110004
#define PNL_KEY_ESCAPE 0x110005

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

typedef struct pnl_app pnl_app_t;
typedef struct pnl_canvas pnl_canvas_t;

typedef void *(*pnl_alloc_fn)(void *ctx, size_t size);
typedef void (*pnl_free_fn)(void *ctx, void *ptr);
typedef void (*pnl_status_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_action_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_timer_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_fd_fn)(void *ctx, pnl_app_t *app, int fd);
typedef void (*pnl_draw_fn)(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas);
typedef int (*pnl_key_fn)(void *ctx, pnl_app_t *app, int key);
typedef void (*pnl_terminal_fn)(void *ctx);

PNL_API int pnl_app_create(const char *title, pnl_alloc_fn alloc_fn, pnl_free_fn free_fn, void *alloc_ctx,
                           pnl_app_t **out_app);
PNL_API void pnl_app_destroy(pnl_app_t *app);
PNL_API int pnl_app_set_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx, int refresh_ms);
PNL_API int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx);
PNL_API int pnl_status_line(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_result(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_app_run(pnl_app_t *app, int argc, char **argv, int *out_exit_code);
PNL_API int pnl_app_quit(pnl_app_t *app);
PNL_API int pnl_app_redraw(pnl_app_t *app);
PNL_API int pnl_app_add_timer(pnl_app_t *app, int interval_ms, pnl_timer_fn timer_fn, void *timer_ctx, int *out_timer);
PNL_API int pnl_app_remove_timer(pnl_app_t *app, int timer);
PNL_API int pnl_app_watch_fd(pnl_app_t *app, int fd, pnl_fd_fn fd_fn, void *fd_ctx);
PNL_API int pnl_app_unwatch_fd(pnl_app_t *app, int fd);
PNL_API int pnl_app_with_terminal(pnl_app_t *app, pnl_terminal_fn terminal_fn, void *terminal_ctx);
PNL_API int pnl_view_push(pnl_app_t *app, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *view_ctx);
PNL_API int pnl_view_pop(pnl_app_t *app);
PNL_API int pnl_canvas_width(const pnl_canvas_t *canvas);
PNL_API int pnl_canvas_height(const pnl_canvas_t *canvas);
PNL_API int pnl_canvas_text(pnl_canvas_t *canvas, int x, int y, int style, const char *text);
PNL_API void pnl_canvas_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int style);
PNL_API int pnl_text_width(const char *text);
PNL_API size_t pnl_key_decode(const unsigned char *data, size_t data_len, int *out_key);
PNL_API const char *pnl_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif
