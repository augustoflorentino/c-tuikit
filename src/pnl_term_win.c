#include "pnl_internal.h"

#if defined(_WIN32)

#include <string.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

#define PNL_TERM_START "\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?1004h"
#define PNL_TERM_MOUSE "\x1b[?1000h\x1b[?1006h"
#define PNL_TERM_END "\x1b[?1004l\x1b[?1006l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"
#define PNL_TERM_RECORDS 32

static DWORD pnl_term_saved_in;
static DWORD pnl_term_saved_out;
static UINT pnl_term_saved_cp_in;
static UINT pnl_term_saved_cp_out;
static volatile LONG pnl_term_raw;
static volatile LONG pnl_term_winch;
static int pnl_term_last_width;
static int pnl_term_last_height;

static HANDLE pnl_term_in(void)
{
    return GetStdHandle(STD_INPUT_HANDLE);
}

static HANDLE pnl_term_out(void)
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

static int pnl_term_is_console(DWORD which)
{
    DWORD mode;

    return GetConsoleMode(GetStdHandle(which), &mode) != 0;
}

static int pnl_term_enable_vt(DWORD which)
{
    HANDLE handle = GetStdHandle(which);
    DWORD mode;

    if (!GetConsoleMode(handle, &mode))
        return 0;
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        return 1;
    return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

int pnl_term_available(void)
{
    return pnl_term_is_console(STD_INPUT_HANDLE) && pnl_term_is_console(STD_OUTPUT_HANDLE);
}

int pnl_term_stdout_is_tty(void)
{
    return pnl_term_enable_vt(STD_OUTPUT_HANDLE);
}

int pnl_term_stderr_is_tty(void)
{
    return pnl_term_enable_vt(STD_ERROR_HANDLE);
}

int pnl_term_stdin_is_tty(void)
{
    return pnl_term_is_console(STD_INPUT_HANDLE);
}

int pnl_os_write(const char *data, size_t data_len)
{
    while (data_len > 0) {
        DWORD chunk = data_len > 0x10000 ? 0x10000 : (DWORD)data_len;
        DWORD written = 0;

        if (!WriteFile(pnl_term_out(), data, chunk, &written, NULL) || written == 0)
            return PNL_ERR_IO;
        data += written;
        data_len -= written;
    }
    return PNL_OK;
}

static int pnl_term_plain;

static void pnl_term_restore(void)
{
    DWORD written;

    if (InterlockedExchange(&pnl_term_raw, 0) == 0)
        return;
    if (!pnl_term_plain)
        WriteFile(pnl_term_out(), PNL_TERM_END, sizeof(PNL_TERM_END) - 1, &written, NULL);
    SetConsoleMode(pnl_term_in(), pnl_term_saved_in);
    SetConsoleMode(pnl_term_out(), pnl_term_saved_out);
    SetConsoleCP(pnl_term_saved_cp_in);
    SetConsoleOutputCP(pnl_term_saved_cp_out);
}

static BOOL WINAPI pnl_term_on_ctrl(DWORD event)
{
    (void)event;
    pnl_term_restore();
    return FALSE;
}

static int pnl_term_make_raw(void)
{
    DWORD in_mode;
    DWORD out_mode;

    if (!GetConsoleMode(pnl_term_in(), &pnl_term_saved_in) || !GetConsoleMode(pnl_term_out(), &pnl_term_saved_out))
        return PNL_ERR_TTY;
    in_mode = pnl_term_saved_in & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT;
    out_mode = pnl_term_saved_out | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
               DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(pnl_term_out(), out_mode))
        return PNL_ERR_UNSUPPORTED;
    if (!SetConsoleMode(pnl_term_in(), in_mode)) {
        SetConsoleMode(pnl_term_out(), pnl_term_saved_out);
        return PNL_ERR_UNSUPPORTED;
    }
    pnl_term_saved_cp_in = GetConsoleCP();
    pnl_term_saved_cp_out = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    InterlockedExchange(&pnl_term_raw, 1);
    return PNL_OK;
}

int pnl_os_enter(struct pnl_term *term)
{
    int status;

    if (term->active)
        return PNL_OK;
    status = pnl_term_make_raw();
    if (status != PNL_OK)
        return status;
    SetConsoleCtrlHandler(pnl_term_on_ctrl, TRUE);
    term->active = 1;
    term->focus_known = 0;
    pnl_os_size(&pnl_term_last_width, &pnl_term_last_height);
    pnl_term_plain = term->linear;
    status = term->linear ? PNL_OK : pnl_os_write(PNL_TERM_START, sizeof(PNL_TERM_START) - 1);
    if (status == PNL_OK && term->mouse && !term->linear)
        status = pnl_os_write(PNL_TERM_MOUSE, sizeof(PNL_TERM_MOUSE) - 1);
    if (status != PNL_OK)
        pnl_os_leave(term);
    return status;
}

void pnl_os_leave(struct pnl_term *term)
{
    if (!term->active)
        return;
    pnl_term_restore();
    SetConsoleCtrlHandler(pnl_term_on_ctrl, FALSE);
    term->active = 0;
    term->input_len = 0;
    term->surrogate = 0;
}

void pnl_os_suspend(struct pnl_term *term)
{
    (void)term;
}

void pnl_os_size(int *out_width, int *out_height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(pnl_term_out(), &info)) {
        *out_width = 80;
        *out_height = 24;
        return;
    }
    *out_width = info.srWindow.Right - info.srWindow.Left + 1;
    *out_height = info.srWindow.Bottom - info.srWindow.Top + 1;
}

int pnl_os_resized(void)
{
    int width;
    int height;

    pnl_os_size(&width, &height);
    if (width != pnl_term_last_width || height != pnl_term_last_height) {
        pnl_term_last_width = width;
        pnl_term_last_height = height;
        InterlockedExchange(&pnl_term_winch, 0);
        return 1;
    }
    return InterlockedExchange(&pnl_term_winch, 0) != 0;
}

static void pnl_term_take_unit(struct pnl_term *term, unsigned int unit)
{
    char bytes[4];
    unsigned int cp = unit;
    size_t len;

    if (unit >= 0xd800 && unit <= 0xdbff) {
        term->surrogate = unit;
        return;
    }
    if (unit >= 0xdc00 && unit <= 0xdfff) {
        if (term->surrogate == 0)
            return;
        cp = 0x10000 + ((term->surrogate - 0xd800) << 10) + (unit - 0xdc00);
    }
    term->surrogate = 0;
    len = pnl_utf8_put(cp, bytes);
    if (term->input_len + len > PNL_INPUT_MAX)
        return;
    memcpy(term->input + term->input_len, bytes, len);
    term->input_len += len;
}

static int pnl_term_drain(struct pnl_term *term)
{
    INPUT_RECORD records[PNL_TERM_RECORDS];
    DWORD count = 0;
    DWORD i;

    if (!GetNumberOfConsoleInputEvents(pnl_term_in(), &count))
        return -1;
    if (count == 0)
        return 0;
    if (!ReadConsoleInputW(pnl_term_in(), records, PNL_TERM_RECORDS, &count))
        return -1;
    for (i = 0; i < count; i++) {
        if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT)
            InterlockedExchange(&pnl_term_winch, 1);
        if (records[i].EventType != KEY_EVENT || !records[i].Event.KeyEvent.bKeyDown)
            continue;
        if (records[i].Event.KeyEvent.uChar.UnicodeChar != 0)
            pnl_term_take_unit(term, records[i].Event.KeyEvent.uChar.UnicodeChar);
    }
    return term->input_len > 0;
}

int pnl_term_fill(struct pnl_term *term, int timeout_ms)
{
    DWORD wait = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
    long long deadline = pnl_os_now_ms() + (timeout_ms < 0 ? 0 : timeout_ms);
    size_t before = term->input_len;

    for (;;) {
        DWORD result = WaitForSingleObject(pnl_term_in(), wait);

        if (result == WAIT_TIMEOUT)
            return 0;
        if (result != WAIT_OBJECT_0 || pnl_term_drain(term) < 0)
            return -1;
        if (term->input_len > before || timeout_ms == 0)
            return term->input_len > before;
        if (timeout_ms > 0) {
            long long left = deadline - pnl_os_now_ms();

            if (left <= 0)
                return 0;
            wait = (DWORD)left;
        }
    }
}

int pnl_os_read_key(struct pnl_term *term, int timeout_ms, int *out_key)
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
    used = pnl_key_decode_at(term->input, term->input_len, out_key, &term->mouse_x, &term->mouse_y);
    memmove(term->input, term->input + used, term->input_len - used);
    term->input_len -= used;
    return PNL_OK;
}

int pnl_term_wake_open(struct pnl_term *term)
{
    if (term->wake_open)
        return PNL_OK;
    term->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (term->wake_event == NULL)
        return PNL_ERR_IO;
    term->wake_open = 1;
    return PNL_OK;
}

void pnl_term_wake_close(struct pnl_term *term)
{
    if (!term->wake_open)
        return;
    CloseHandle((HANDLE)term->wake_event);
    term->wake_event = NULL;
    term->wake_open = 0;
}

void pnl_os_wake(struct pnl_term *term)
{
    if (term->wake_open)
        SetEvent((HANDLE)term->wake_event);
}

int pnl_os_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len, int timeout_ms,
                  unsigned char *out_ready)
{
    (void)watches;
    (void)watches_len;
    (void)out_ready;
    if (term->input_len > 0)
        return 1;
    if (term->wake_open) {
        HANDLE handles[2];
        DWORD result;

        handles[0] = pnl_term_in();
        handles[1] = (HANDLE)term->wake_event;
        result = WaitForMultipleObjects(2, handles, FALSE, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);
        if (result == WAIT_OBJECT_0 + 1)
            term->woken = 1;
        if (result != WAIT_OBJECT_0)
            return result == WAIT_FAILED ? -1 : 0;
        timeout_ms = 0;
    }
    return pnl_term_fill(term, timeout_ms) < 0 ? -1 : term->input_len > 0;
}

void pnl_nap_ms(int ms)
{
    Sleep((DWORD)ms);
}

long long pnl_os_now_ms(void)
{
    return (long long)GetTickCount64();
}

#endif
