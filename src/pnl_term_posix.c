#include "pnl_internal.h"

#if !defined(_WIN32)

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define PNL_TERM_START "\x1b[?1049h\x1b[?25l\x1b[?7l"
#define PNL_TERM_END "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"

static const int pnl_term_signals[] = {SIGTERM, SIGHUP, SIGQUIT, SIGINT};
#define PNL_TERM_SIGNALS (sizeof(pnl_term_signals) / sizeof(pnl_term_signals[0]))

static struct termios pnl_term_saved;
static volatile sig_atomic_t pnl_term_raw;
static volatile sig_atomic_t pnl_term_winch;
static struct sigaction pnl_term_old[PNL_TERM_SIGNALS];
static struct sigaction pnl_term_old_winch;

static void pnl_term_restore(void)
{
    if (!pnl_term_raw)
        return;
    pnl_term_raw = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &pnl_term_saved);
    if (write(STDOUT_FILENO, PNL_TERM_END, sizeof(PNL_TERM_END) - 1) < 0)
        return;
}

static void pnl_term_on_signal(int sig)
{
    pnl_term_restore();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void pnl_term_on_winch(int sig)
{
    (void)sig;
    pnl_term_winch = 1;
}

static void pnl_term_catch(void)
{
    struct sigaction sa;
    size_t i;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = pnl_term_on_signal;
    for (i = 0; i < PNL_TERM_SIGNALS; i++)
        sigaction(pnl_term_signals[i], &sa, &pnl_term_old[i]);
    sa.sa_handler = pnl_term_on_winch;
    sigaction(SIGWINCH, &sa, &pnl_term_old_winch);
}

static void pnl_term_release(void)
{
    size_t i;

    for (i = 0; i < PNL_TERM_SIGNALS; i++)
        sigaction(pnl_term_signals[i], &pnl_term_old[i], NULL);
    sigaction(SIGWINCH, &pnl_term_old_winch, NULL);
}

int pnl_term_available(void)
{
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

int pnl_term_stdout_is_tty(void)
{
    return isatty(STDOUT_FILENO);
}

int pnl_term_stderr_is_tty(void)
{
    return isatty(STDERR_FILENO);
}

int pnl_term_stdin_is_tty(void)
{
    return isatty(STDIN_FILENO);
}

static int pnl_term_make_raw(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &pnl_term_saved) != 0)
        return PNL_ERR_TTY;
    raw = pnl_term_saved;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(tcflag_t)OPOST;
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        return PNL_ERR_TTY;
    pnl_term_raw = 1;
    return PNL_OK;
}

int pnl_term_enter(struct pnl_term *term)
{
    int status;

    if (term->active)
        return PNL_OK;
    status = pnl_term_make_raw();
    if (status != PNL_OK)
        return status;
    pnl_term_catch();
    term->active = 1;
    status = pnl_term_write(PNL_TERM_START, sizeof(PNL_TERM_START) - 1);
    if (status != PNL_OK)
        pnl_term_leave(term);
    return status;
}

void pnl_term_leave(struct pnl_term *term)
{
    if (!term->active)
        return;
    pnl_term_restore();
    pnl_term_release();
    term->active = 0;
    term->input_len = 0;
}

void pnl_term_suspend(struct pnl_term *term)
{
    pnl_term_leave(term);
    raise(SIGTSTP);
    pnl_term_enter(term);
}

int pnl_term_resized(void)
{
    if (!pnl_term_winch)
        return 0;
    pnl_term_winch = 0;
    return 1;
}

void pnl_term_size(int *out_width, int *out_height)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0 || ws.ws_row == 0) {
        *out_width = 80;
        *out_height = 24;
        return;
    }
    *out_width = ws.ws_col;
    *out_height = ws.ws_row;
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

static int pnl_term_poll_input(int timeout_ms)
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

int pnl_term_fill(struct pnl_term *term, int timeout_ms)
{
    ssize_t got;
    int ready = pnl_term_poll_input(timeout_ms);

    if (ready <= 0)
        return ready;
    got = read(STDIN_FILENO, term->input + term->input_len, PNL_INPUT_MAX - term->input_len);
    if (got < 0 && (errno == EINTR || errno == EAGAIN))
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

int pnl_term_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len, int timeout_ms,
                  unsigned char *out_ready)
{
    struct pollfd pfds[PNL_WATCH_MAX + 1];
    size_t count = watches_len < PNL_WATCH_MAX ? watches_len : PNL_WATCH_MAX;
    size_t i;
    int ready;

    if (term->input_len > 0)
        timeout_ms = 0;
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    for (i = 0; i < count; i++) {
        pfds[i + 1].fd = watches[i].fd;
        pfds[i + 1].events = POLLIN;
    }
    for (i = 0; i <= count; i++)
        pfds[i].revents = 0;
    ready = poll(pfds, (nfds_t)(count + 1), timeout_ms);
    if (ready < 0)
        return errno == EINTR ? 0 : -1;
    for (i = 0; i < count; i++)
        out_ready[i] = (unsigned char)(pfds[i + 1].revents != 0);
    return term->input_len > 0 || pfds[0].revents != 0;
}

long long pnl_term_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif
