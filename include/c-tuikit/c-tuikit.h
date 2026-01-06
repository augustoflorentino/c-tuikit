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

typedef struct pnl_app pnl_app_t;

typedef void *(*pnl_alloc_fn)(void *ctx, size_t size);
typedef void (*pnl_free_fn)(void *ctx, void *ptr);
typedef void (*pnl_status_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_action_fn)(void *ctx, pnl_app_t *app);

PNL_API int pnl_app_create(const char *title, pnl_alloc_fn alloc_fn, pnl_free_fn free_fn, void *alloc_ctx,
                           pnl_app_t **out_app);
PNL_API void pnl_app_destroy(pnl_app_t *app);
PNL_API int pnl_app_set_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx, int refresh_ms);
PNL_API int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx);
PNL_API int pnl_status_line(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_result(pnl_app_t *app, int mark, const char *text);
PNL_API int pnl_app_run(pnl_app_t *app);
PNL_API size_t pnl_key_decode(const unsigned char *data, size_t data_len, int *out_key);
PNL_API const char *pnl_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif
