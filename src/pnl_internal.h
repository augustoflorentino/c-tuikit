#ifndef PNL_INTERNAL_H
#define PNL_INTERNAL_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "c-tuikit/c-tuikit.h"

#if defined(PNL_IMPLEMENTATION_DONE)
#define PNL_INTERNAL static
#else
#define PNL_INTERNAL
#endif

#define PNL_INPUT_MAX 64
#define PNL_ESCAPE_WAIT_MS 50

struct pnl_term {
    int active;
    unsigned char input[PNL_INPUT_MAX];
    size_t input_len;
    void *saved;
};

PNL_INTERNAL int pnl_term_available(void);
PNL_INTERNAL int pnl_term_enter(struct pnl_term *term, pnl_alloc_fn alloc_fn, void *alloc_ctx);
PNL_INTERNAL void pnl_term_leave(struct pnl_term *term, pnl_free_fn free_fn, void *alloc_ctx);
PNL_INTERNAL int pnl_term_write(const char *data, size_t data_len);
PNL_INTERNAL int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key);

#endif
