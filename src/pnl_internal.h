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
#define PNL_ESCAPE_WAIT_MS 50
#define PNL_KEY_SUSPEND 0x1a

#define PNL_ATTR_BOLD 0x01u
#define PNL_ATTR_DIM 0x02u
#define PNL_ATTR_REVERSE 0x04u
#define PNL_ATTR_UNDERLINE 0x08u
#define PNL_INK_DEFAULT 0u
#define PNL_INK_RED 2u
#define PNL_INK_GREEN 3u
#define PNL_INK_YELLOW 4u
#define PNL_INK_GRAY 9u
#define PNL_PACK(fg, bg, attrs) ((uint32_t)(attrs) | ((uint32_t)(fg) << 8) | ((uint32_t)(bg) << 16))

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
    unsigned char input[PNL_INPUT_MAX];
    size_t input_len;
};

PNL_INTERNAL void *pnl_mem_alloc(const struct pnl_mem *mem, size_t size);
PNL_INTERNAL void pnl_mem_free(const struct pnl_mem *mem, void *ptr);
PNL_INTERNAL char *pnl_mem_copy(const struct pnl_mem *mem, const char *text);
PNL_INTERNAL int pnl_mem_grow(const struct pnl_mem *mem, void **items, size_t *cap, size_t needed, size_t item_size);

PNL_INTERNAL size_t pnl_utf8_next(const char *text, size_t text_len, uint32_t *out_cp);
PNL_INTERNAL int pnl_cp_width(uint32_t cp);
PNL_INTERNAL int pnl_text_width(const char *text);

PNL_INTERNAL int pnl_screen_resize(struct pnl_screen *screen, const struct pnl_mem *mem, int width, int height);
PNL_INTERNAL void pnl_screen_free(struct pnl_screen *screen, const struct pnl_mem *mem);
PNL_INTERNAL void pnl_screen_clear(struct pnl_screen *screen);
PNL_INTERNAL int pnl_screen_text(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text);
PNL_INTERNAL void pnl_screen_fill(struct pnl_screen *screen, int x, int y, int width, uint32_t style);
PNL_INTERNAL int pnl_screen_render(struct pnl_screen *screen, const struct pnl_mem *mem);

PNL_INTERNAL int pnl_term_available(void);
PNL_INTERNAL int pnl_term_enter(struct pnl_term *term);
PNL_INTERNAL void pnl_term_leave(struct pnl_term *term);
PNL_INTERNAL void pnl_term_suspend(struct pnl_term *term);
PNL_INTERNAL int pnl_term_resized(void);
PNL_INTERNAL void pnl_term_size(int *out_width, int *out_height);
PNL_INTERNAL int pnl_term_write(const char *data, size_t data_len);
PNL_INTERNAL int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key);

#endif
