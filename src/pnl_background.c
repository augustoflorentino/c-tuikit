#include "pnl_internal.h"

#include <stdlib.h>
#include <string.h>

#define PNL_QUERY "\x1b]11;?\x1b\\\x1b[c"
#define PNL_QUERY_WAIT_MS 150

static int pnl_hex(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static long pnl_parse_channel(const unsigned char *text, size_t len, size_t *pos)
{
    long value = 0;
    size_t digits = 0;

    while (*pos < len && pnl_hex(text[*pos]) >= 0) {
        if (digits < 2)
            value = value * 16 + pnl_hex(text[*pos]);
        digits++;
        (*pos)++;
    }
    if (digits == 0)
        return -1;
    return digits == 1 ? value * 17 : value;
}

static long pnl_parse_rgb(const unsigned char *text, size_t len)
{
    static const char prefix[] = "rgb:";
    size_t pos = sizeof(prefix) - 1;
    long rgb = 0;
    int i;

    if (len < pos || memcmp(text, prefix, pos) != 0)
        return -1;
    for (i = 0; i < 3; i++) {
        long channel = pnl_parse_channel(text, len, &pos);

        if (channel < 0 || (i < 2 && (pos >= len || text[pos++] != '/')))
            return -1;
        rgb = rgb << 8 | channel;
    }
    return rgb;
}

static size_t pnl_find(const struct pnl_term *term, const char *needle, size_t from)
{
    size_t len = strlen(needle);
    size_t i;

    for (i = from; i + len <= term->input_len; i++) {
        if (memcmp(term->input + i, needle, len) == 0)
            return i;
    }
    return (size_t)-1;
}

static void pnl_cut(struct pnl_term *term, size_t from, size_t to)
{
    memmove(term->input + from, term->input + to, term->input_len - to);
    term->input_len -= to - from;
}

static long pnl_take_osc(struct pnl_term *term)
{
    size_t start = pnl_find(term, "\x1b]11;", 0);
    size_t end;
    size_t body;
    long rgb;

    if (start == (size_t)-1)
        return -2;
    body = start + 5;
    for (end = body; end < term->input_len && term->input[end] != 0x07 && term->input[end] != 0x1b; end++)
        ;
    if (end >= term->input_len || (term->input[end] == 0x1b && end + 1 >= term->input_len))
        return -2;
    rgb = pnl_parse_rgb(term->input + body, end - body);
    pnl_cut(term, start, end + (term->input[end] == 0x1b ? 2 : 1));
    return rgb;
}

static int pnl_take_da(struct pnl_term *term)
{
    size_t start = pnl_find(term, "\x1b[?", 0);
    size_t end;

    if (start == (size_t)-1)
        return 0;
    for (end = start + 3; end < term->input_len && term->input[end] != 'c'; end++) {
        if (term->input[end] != ';' && (term->input[end] < '0' || term->input[end] > '9'))
            return 0;
    }
    if (end >= term->input_len)
        return 0;
    pnl_cut(term, start, end + 1);
    return 1;
}

static int pnl_background_env(long *out_rgb)
{
    const char *forced = getenv("TUIKIT_BACKGROUND");
    const char *fgbg = getenv("COLORFGBG");
    const char *last;

    if (forced != NULL && (strcmp(forced, "light") == 0 || strcmp(forced, "dark") == 0)) {
        *out_rgb = forced[0] == 'l' ? 0xffffffL : 0x000000L;
        return 1;
    }
    if (fgbg == NULL || (last = (strrchr)(fgbg, ';')) == NULL)
        return 0;
    *out_rgb = atoi(last + 1) == 7 || atoi(last + 1) == 15 ? 0xffffffL : 0x000000L;
    return 1;
}

static long pnl_query(struct pnl_term *term, const char *query, size_t query_len, long *out_rgb, long *out_reply_ms)
{
    long long sent = pnl_term_now_ms();
    long long deadline = sent + PNL_QUERY_WAIT_MS;
    long rgb = -2;

    *out_reply_ms = -1;
    if (pnl_term_write(query, query_len) != PNL_OK)
        return -1;
    for (;;) {
        long long left = deadline - pnl_term_now_ms();
        int answered;

        rgb = pnl_take_osc(term);
        answered = rgb != -2 || pnl_take_da(term);
        if (answered && *out_reply_ms < 0)
            *out_reply_ms = (long)(pnl_term_now_ms() - sent);
        if (answered || left <= 0 || pnl_term_fill(term, (int)left) <= 0)
            break;
    }
    pnl_take_da(term);
    *out_rgb = rgb;
    return rgb;
}

long pnl_link_probe(struct pnl_term *term)
{
    long rgb;
    long reply_ms;

    pnl_query(term, "\x1b[c", 3, &rgb, &reply_ms);
    return reply_ms;
}

int pnl_background_detect(struct pnl_term *term, int depth, long *out_rgb, long *out_reply_ms)
{
    long rgb;

    *out_rgb = -1;
    *out_reply_ms = -1;
    if (pnl_background_env(out_rgb))
        return 1;
    if (depth == PNL_DEPTH_NONE)
        return 0;
    pnl_query(term, PNL_QUERY, sizeof(PNL_QUERY) - 1, &rgb, out_reply_ms);
    if (rgb < 0)
        return 0;
    *out_rgb = rgb;
    return 1;
}
