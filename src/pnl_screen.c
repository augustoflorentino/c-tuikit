#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_CONTINUATION 0u
#define PNL_BLANK ' '

int pnl_screen_resize(struct pnl_screen *screen, const struct pnl_mem *mem, int width, int height)
{
    size_t count = (size_t)width * (size_t)height;
    struct pnl_cell *back;
    struct pnl_cell *front;

    if (width <= 0 || height <= 0)
        return PNL_ERR_ARG;
    if (screen->link_pool == NULL) {
        screen->link_pool = pnl_mem_alloc(mem, PNL_LINK_POOL + PNL_LINKS * sizeof(*screen->link_at));
        if (screen->link_pool == NULL)
            return PNL_ERR_NOMEM;
        screen->link_at = (unsigned short *)(void *)(screen->link_pool + PNL_LINK_POOL);
    }
    if (width == screen->width && height == screen->height)
        return PNL_OK;
    back = pnl_mem_alloc(mem, count * sizeof(*back));
    front = pnl_mem_alloc(mem, count * sizeof(*front));
    if (back == NULL || front == NULL) {
        pnl_mem_free(mem, back);
        pnl_mem_free(mem, front);
        return PNL_ERR_NOMEM;
    }
    pnl_mem_free(mem, screen->back);
    pnl_mem_free(mem, screen->front);
    screen->back = back;
    screen->front = front;
    screen->width = width;
    screen->height = height;
    screen->front_valid = 0;
    pnl_screen_clear(screen);
    return PNL_OK;
}

void pnl_screen_free(struct pnl_screen *screen, const struct pnl_mem *mem)
{
    pnl_mem_free(mem, screen->back);
    pnl_mem_free(mem, screen->front);
    pnl_mem_free(mem, screen->out);
    pnl_mem_free(mem, screen->link_pool);
    memset(screen, 0, sizeof(*screen));
}

void pnl_screen_clear(struct pnl_screen *screen)
{
    size_t count = (size_t)screen->width * (size_t)screen->height;
    size_t i;

    for (i = 0; i < count; i++) {
        screen->back[i].cp = PNL_BLANK;
        screen->back[i].style = 0;
    }
    screen->links_len = 0;
    screen->link_pool_len = 0;
    screen->links_hash = 5381;
}

static uint32_t pnl_link_add(struct pnl_screen *screen, const char *url, size_t url_len)
{
    int i;

    if (screen->links_off || url_len == 0 || screen->link_pool == NULL)
        return 0;
    for (i = 0; i < screen->links_len; i++) {
        const char *known = screen->link_pool + screen->link_at[i];

        if (strncmp(known, url, url_len) == 0 && known[url_len] == '\0')
            return (uint32_t)(i + 1) << 24;
    }
    if (screen->links_len == PNL_LINKS || screen->link_pool_len + url_len + 1 > PNL_LINK_POOL)
        return 0;
    screen->link_at[screen->links_len++] = (unsigned short)screen->link_pool_len;
    memcpy(screen->link_pool + screen->link_pool_len, url, url_len);
    screen->link_pool_len += url_len;
    screen->link_pool[screen->link_pool_len++] = '\0';
    for (i = 0; i < (int)url_len; i++)
        screen->links_hash = screen->links_hash * 33u + (unsigned char)url[i];
    screen->links_hash = screen->links_hash * 33u + (unsigned long)screen->links_len;
    return (uint32_t)screen->links_len << 24;
}

static size_t pnl_url_length(const char *text, size_t len)
{
    size_t end;

    if (!(len > 8 && strncmp(text, "https://", 8) == 0) && !(len > 7 && strncmp(text, "http://", 7) == 0))
        return 0;
    for (end = 0; end < len && (unsigned char)text[end] > 0x20 && text[end] != 0x7f && text[end] != '"' &&
                  text[end] != '<' && text[end] != '>';
         end++)
        ;
    while (end > 0 && (strchr)(".,;:!?)]'", text[end - 1]) != NULL)
        end--;
    return end;
}

static struct pnl_cell *pnl_screen_at(struct pnl_screen *screen, int x, int y)
{
    return &screen->back[(size_t)y * (size_t)screen->width + (size_t)x];
}

static void pnl_screen_put(struct pnl_screen *screen, int x, int y, uint32_t cp, uint32_t style)
{
    struct pnl_cell *cell = pnl_screen_at(screen, x, y);

    if (cell->cp == PNL_CONTINUATION && x > 0)
        pnl_screen_at(screen, x - 1, y)->cp = PNL_BLANK;
    if (x + 1 < screen->width && pnl_screen_at(screen, x + 1, y)->cp == PNL_CONTINUATION)
        pnl_screen_at(screen, x + 1, y)->cp = PNL_BLANK;
    cell->cp = cp;
    cell->style = style;
}

int pnl_screen_link(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text,
                    const char *url)
{
    size_t len = strlen(text);
    size_t pos = 0;
    size_t link_end = 0;
    uint32_t link = 0;
    int end;

    if (y < 0 || y >= screen->height || x < 0 || x >= screen->width)
        return 0;
    if (url != NULL) {
        link = pnl_link_add(screen, url, strlen(url));
        link_end = len;
    }
    end = max >= 0 && x + max < screen->width ? x + max : screen->width;
    while (pos < len && x < end) {
        uint32_t cp;
        int w;

        if (pos >= link_end) {
            link_end = url == NULL && (text[pos] == 'h') ? pnl_url_length(text + pos, len - pos) : 0;
            link = link_end > 0 ? pnl_link_add(screen, text + pos, link_end) : 0;
            link_end += pos;
        }
        pos += pnl_utf8_next(text + pos, len - pos, &cp);
        w = pnl_cp_width(cp);
        if (w == 0)
            continue;
        if (w < 0) {
            cp = 0xfffd;
            w = 1;
        }
        if (x + w > end)
            break;
        pnl_screen_put(screen, x, y, cp, (style & 0xffffffu) | link);
        if (w == 2)
            pnl_screen_put(screen, x + 1, y, PNL_CONTINUATION, (style & 0xffffffu) | link);
        x += w;
    }
    return x;
}

int pnl_screen_text(struct pnl_screen *screen, int x, int y, int max, uint32_t style, const char *text)
{
    return pnl_screen_link(screen, x, y, max, style, text, NULL);
}

void pnl_screen_fill(struct pnl_screen *screen, int x, int y, int width, uint32_t style)
{
    int end = x + width < screen->width ? x + width : screen->width;

    if (y < 0 || y >= screen->height)
        return;
    for (x = x < 0 ? 0 : x; x < end; x++)
        pnl_screen_put(screen, x, y, PNL_BLANK, style);
}

static int pnl_out(struct pnl_screen *screen, const struct pnl_mem *mem, const char *data, size_t data_len)
{
    int status = pnl_mem_grow(mem, (void **)&screen->out, &screen->out_cap, screen->out_len + data_len, 1);

    if (status != PNL_OK)
        return status;
    memcpy(screen->out + screen->out_len, data, data_len);
    screen->out_len += data_len;
    return PNL_OK;
}

static int pnl_out_style(struct pnl_screen *screen, const struct pnl_mem *mem, uint32_t style)
{
    char buf[64] = "\x1b[0";
    int len = 3;

    if (style & PNL_BOLD)
        len += sprintf(buf + len, ";1");
    if (style & PNL_DIM)
        len += sprintf(buf + len, ";2");
    if (style & PNL_UNDERLINE)
        len += sprintf(buf + len, ";4");
    if ((style & PNL_REVERSE) || (screen->palette.depth == PNL_DEPTH_NONE && PNL_STYLE_BG(style) != PNL_COLOR_DEFAULT))
        len += sprintf(buf + len, ";7");
    if (screen->palette.depth != PNL_DEPTH_NONE) {
        len += pnl_color_ink(buf + len, PNL_STYLE_FG(style), 0, &screen->palette);
        len += pnl_color_ink(buf + len, PNL_STYLE_BG(style), 1, &screen->palette);
    }
    buf[len++] = 'm';
    return pnl_out(screen, mem, buf, (size_t)len);
}

static int pnl_out_cp(struct pnl_screen *screen, const struct pnl_mem *mem, uint32_t cp)
{
    char buf[4];
    size_t len;

    if (cp < 0x80) {
        buf[0] = (char)cp;
        len = 1;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xc0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3f));
        len = 2;
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xe0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        buf[2] = (char)(0x80 | (cp & 0x3f));
        len = 3;
    } else {
        buf[0] = (char)(0xf0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
        buf[3] = (char)(0x80 | (cp & 0x3f));
        len = 4;
    }
    return pnl_out(screen, mem, buf, len);
}

static int pnl_cell_same(const struct pnl_cell *a, const struct pnl_cell *b)
{
    return a->cp == b->cp && a->style == b->style;
}

struct pnl_pen {
    int x;
    int y;
    uint32_t style;
    int style_known;
    uint32_t link;
};

static int pnl_out_link(struct pnl_screen *screen, const struct pnl_mem *mem, struct pnl_pen *pen, uint32_t link)
{
    const char *url = link > 0 ? screen->link_pool + screen->link_at[link - 1] : "";
    int status;

    pen->link = link;
    status = pnl_out(screen, mem, "\x1b]8;;", 5);
    if (status == PNL_OK)
        status = pnl_out(screen, mem, url, strlen(url));
    if (status == PNL_OK)
        status = pnl_out(screen, mem, "\x1b\\", 2);
    return status;
}

static int pnl_render_cell(struct pnl_screen *screen, const struct pnl_mem *mem, struct pnl_pen *pen, int x, int y)
{
    const struct pnl_cell *cell = pnl_screen_at(screen, x, y);
    int status = PNL_OK;

    if (pen->x != x || pen->y != y) {
        char buf[32];
        int len = sprintf(buf, "\x1b[%d;%dH", y + 1, x + 1);

        status = pnl_out(screen, mem, buf, (size_t)len);
    }
    if (status == PNL_OK && pen->link != PNL_STYLE_LINK(cell->style))
        status = pnl_out_link(screen, mem, pen, PNL_STYLE_LINK(cell->style));
    if (status == PNL_OK && (!pen->style_known || pen->style != cell->style))
        status = pnl_out_style(screen, mem, cell->style);
    if (status == PNL_OK)
        status = pnl_out_cp(screen, mem, cell->cp);
    pen->style = cell->style;
    pen->style_known = 1;
    pen->y = y;
    pen->x = x + (pnl_cp_width(cell->cp) == 2 ? 2 : 1);
    return status;
}

static int pnl_render_row(struct pnl_screen *screen, const struct pnl_mem *mem, struct pnl_pen *pen, int y)
{
    int x;
    int status = PNL_OK;

    for (x = 0; x < screen->width && status == PNL_OK; x++) {
        size_t at = (size_t)y * (size_t)screen->width + (size_t)x;
        int wide = x + 1 < screen->width && screen->back[at + 1].cp == PNL_CONTINUATION;

        if (screen->back[at].cp == PNL_CONTINUATION)
            continue;
        if (screen->front_valid && pnl_cell_same(&screen->back[at], &screen->front[at]) &&
            (!wide || pnl_cell_same(&screen->back[at + 1], &screen->front[at + 1])))
            continue;
        status = pnl_render_cell(screen, mem, pen, x, y);
    }
    return status;
}

static int pnl_linear_same(const struct pnl_screen *screen, int y, int from)
{
    const struct pnl_cell *old = screen->front + (size_t)from * (size_t)screen->width;
    const struct pnl_cell *now = screen->back + (size_t)y * (size_t)screen->width;
    int x;

    for (x = 0; x < screen->width; x++) {
        if (old[x].cp != now[x].cp && !(from == y && old[x].cp == '>' && now[x].cp == ' '))
            return 0;
    }
    return 1;
}

static int pnl_linear_seen(const struct pnl_screen *screen, int y)
{
    int from;

    for (from = 0; from < screen->height; from++) {
        if (pnl_linear_same(screen, y, from))
            return 1;
    }
    return 0;
}

static int pnl_linear_box(uint32_t cp)
{
    return cp >= 0x2500 && cp <= 0x257f;
}

static int pnl_linear_skip(uint32_t cp)
{
    return cp == PNL_CONTINUATION || (cp >= 0x2500 && cp <= 0x259f);
}

static int pnl_linear_row(struct pnl_screen *screen, const struct pnl_mem *mem, int y)
{
    const struct pnl_cell *row = screen->back + (size_t)y * (size_t)screen->width;
    int status = PNL_OK;
    int spaces = 0;
    int written = 0;
    int start = 0;
    int end = screen->width;
    int x;

    while (start < screen->width && !pnl_linear_box(row[start].cp))
        start++;
    while (end > start + 1 && !pnl_linear_box(row[end - 1].cp))
        end--;
    if (end <= start + 1) {
        start = 0;
        end = screen->width;
    }
    for (x = start; x < end && status == PNL_OK; x++) {
        uint32_t cp = pnl_linear_skip(row[x].cp) ? ' ' : row[x].cp;

        if (cp == ' ') {
            spaces++;
            continue;
        }
        if (written && spaces > 0)
            status = pnl_out(screen, mem, "  ", spaces > 1 ? 2 : 1);
        if (status == PNL_OK)
            status = pnl_out_cp(screen, mem, cp);
        spaces = 0;
        written = 1;
    }
    if (status == PNL_OK && written)
        status = pnl_out(screen, mem, "\r\n", 2);
    return status;
}

static int pnl_linear_render(struct pnl_screen *screen, const struct pnl_mem *mem)
{
    int status = PNL_OK;
    int y;

    for (y = 0; y < screen->height && status == PNL_OK; y++) {
        if (!screen->front_valid || !pnl_linear_seen(screen, y))
            status = pnl_linear_row(screen, mem, y);
    }
    return status;
}

int pnl_screen_render(struct pnl_screen *screen, const struct pnl_mem *mem)
{
    struct pnl_pen pen = {-1, -1, 0, 0, 0};
    size_t count = (size_t)screen->width * (size_t)screen->height;
    int status = PNL_OK;
    int y;

    screen->out_len = 0;
    if (screen->links_hash != screen->links_drawn)
        screen->front_valid = 0;
    screen->links_drawn = screen->links_hash;
    if (screen->linear)
        status = pnl_linear_render(screen, mem);
    else if (!screen->front_valid)
        status = pnl_out(screen, mem, "\x1b[0m\x1b[2J", 8);
    for (y = 0; y < screen->height && status == PNL_OK && !screen->linear; y++)
        status = pnl_render_row(screen, mem, &pen, y);
    if (status == PNL_OK && pen.link > 0)
        status = pnl_out_link(screen, mem, &pen, 0);
    if (status == PNL_OK && pen.style_known)
        status = pnl_out(screen, mem, "\x1b[0m", 4);
    if (status == PNL_OK && screen->out_len > 0)
        status = pnl_term_write(screen->out, screen->out_len);
    if (status == PNL_OK && screen->out_len > 0)
        pnl_record_output(screen->out, screen->out_len, screen->width, screen->height);
    if (status != PNL_OK) {
        screen->front_valid = 0;
        return status;
    }
    memcpy(screen->front, screen->back, count * sizeof(*screen->front));
    screen->front_valid = 1;
    return PNL_OK;
}
