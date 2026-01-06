#include "pnl_internal.h"

#if defined(_WIN32)

int pnl_term_available(void)
{
    return 0;
}

int pnl_term_enter(struct pnl_term *term, pnl_alloc_fn alloc_fn, void *alloc_ctx)
{
    (void)term;
    (void)alloc_fn;
    (void)alloc_ctx;
    return PNL_ERR_TTY;
}

void pnl_term_leave(struct pnl_term *term, pnl_free_fn free_fn, void *alloc_ctx)
{
    (void)term;
    (void)free_fn;
    (void)alloc_ctx;
}

int pnl_term_write(const char *data, size_t data_len)
{
    (void)data;
    (void)data_len;
    return PNL_ERR_TTY;
}

int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key)
{
    (void)term;
    (void)timeout_ms;
    *out_key = PNL_KEY_NONE;
    return PNL_ERR_TTY;
}

#else

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int pnl_term_available(void)
{
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

int pnl_term_enter(struct pnl_term *term, pnl_alloc_fn alloc_fn, void *alloc_ctx)
{
    struct termios *saved;
    struct termios raw;

    if (term->active)
        return PNL_OK;
    saved = term->saved;
    if (saved == NULL) {
        saved = alloc_fn(alloc_ctx, sizeof(*saved));
        if (saved == NULL)
            return PNL_ERR_NOMEM;
        term->saved = saved;
    }
    if (tcgetattr(STDIN_FILENO, saved) != 0)
        return PNL_ERR_TTY;
    raw = *saved;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(tcflag_t)OPOST;
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        return PNL_ERR_TTY;
    term->active = 1;
    return PNL_OK;
}

void pnl_term_leave(struct pnl_term *term, pnl_free_fn free_fn, void *alloc_ctx)
{
    if (term->active)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, (struct termios *)term->saved);
    term->active = 0;
    term->input_len = 0;
    if (free_fn != NULL && term->saved != NULL) {
        free_fn(alloc_ctx, term->saved);
        term->saved = NULL;
    }
}

int pnl_term_write(const char *data, size_t data_len)
{
    while (data_len > 0) {
        ssize_t written = write(STDOUT_FILENO, data, data_len);

        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return PNL_ERR_IO;
        data += written;
        data_len -= (size_t)written;
    }
    return PNL_OK;
}

static int pnl_term_wait(int timeout_ms)
{
    struct pollfd pfd;
    int ready;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    ready = poll(&pfd, 1, timeout_ms);
    if (ready < 0 && errno == EINTR)
        return 0;
    return ready;
}

static int pnl_term_fill(struct pnl_term *term, int timeout_ms)
{
    ssize_t got;
    int ready = pnl_term_wait(timeout_ms);

    if (ready <= 0)
        return ready;
    got = read(STDIN_FILENO, term->input + term->input_len, PNL_INPUT_MAX - term->input_len);
    if (got < 0 && errno == EINTR)
        return 0;
    if (got <= 0)
        return -1;
    term->input_len += (size_t)got;
    return 1;
}

int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key)
{
    size_t used;

    *out_key = PNL_KEY_NONE;
    if (term->input_len == 0) {
        int filled = pnl_term_fill(term, timeout_ms);

        if (filled <= 0)
            return filled < 0 ? PNL_ERR_IO : PNL_OK;
    }
    if (term->input_len == 1 && term->input[0] == 0x1b && pnl_term_fill(term, PNL_ESCAPE_WAIT_MS) < 0)
        return PNL_ERR_IO;
    used = pnl_key_decode(term->input, term->input_len, out_key);
    memmove(term->input, term->input + used, term->input_len - used);
    term->input_len -= used;
    return PNL_OK;
}

#endif
