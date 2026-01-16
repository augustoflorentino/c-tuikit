#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_PAGER_TAB 4
#define PNL_SEARCH_MAX 128
#define PNL_PAGER_FOOTER_MAX 192

struct pnl_pager {
    char *text;
    size_t *lines;
    size_t lines_len;
    size_t top;
    size_t page;
    size_t match;
    int has_match;
    int searching;
    char search[PNL_SEARCH_MAX];
    struct pnl_edit edit;
    char footer_text[PNL_PAGER_FOOTER_MAX];
    const char *footer;
};

static size_t pnl_pager_expanded_len(const char *text, size_t text_len)
{
    size_t len = 0;
    size_t i;

    for (i = 0; i < text_len; i++)
        len += text[i] == '\t' ? PNL_PAGER_TAB : text[i] == '\0' ? 0 : 1;
    return len + 1;
}

static size_t pnl_pager_copy(struct pnl_pager *pager, const char *text, size_t text_len)
{
    size_t out = 0;
    size_t i;
    size_t lines = 1;

    for (i = 0; i < text_len; i++) {
        if (text[i] == '\t') {
            memset(pager->text + out, ' ', PNL_PAGER_TAB);
            out += PNL_PAGER_TAB;
        } else if (text[i] != '\0' && text[i] != '\r') {
            pager->text[out++] = text[i];
        }
        lines += text[i] == '\n';
    }
    pager->text[out] = '\0';
    return out > 0 && pager->text[out - 1] == '\n' ? lines - 1 : lines;
}

static void pnl_pager_split(struct pnl_pager *pager)
{
    char *pos = pager->text;
    size_t i;

    for (i = 0; i < pager->lines_len; i++) {
        char *end = (strchr)(pos, '\n');

        pager->lines[i] = (size_t)(pos - pager->text);
        if (end == NULL)
            break;
        *end = '\0';
        pos = end + 1;
    }
}

static const char *pnl_pager_line(const struct pnl_pager *pager, size_t index)
{
    return pager->text + pager->lines[index];
}

static size_t pnl_pager_last_top(const struct pnl_pager *pager)
{
    return pager->lines_len > pager->page ? pager->lines_len - pager->page : 0;
}

static void pnl_pager_status(struct pnl_pager *pager)
{
    size_t bottom = pager->top + pager->page < pager->lines_len ? pager->top + pager->page : pager->lines_len;

    if (pager->searching) {
        snprintf(pager->footer_text, sizeof(pager->footer_text), "/%s_   enter: find  esc: cancel", pager->search);
    } else {
        snprintf(pager->footer_text, sizeof(pager->footer_text),
                 "%zu-%zu of %zu%s   up/down: scroll  /: search  n/N: next/previous  q: back",
                 pager->lines_len ? pager->top + 1 : 0, bottom, pager->lines_len,
                 pager->search[0] && !pager->has_match ? "  (not found)" : "");
    }
}

static void pnl_pager_draw_line(struct pnl_pager *pager, pnl_canvas_t *canvas, int y, size_t index)
{
    const char *line = pnl_pager_line(pager, index);
    const char *hit;
    size_t needle = strlen(pager->search);
    char head[1024];
    int x;

    pnl_canvas_text(canvas, 1, y, 0, line);
    if (!pager->has_match || index != pager->match || needle == 0)
        return;
    for (hit = line; *hit != '\0' && !pnl_text_contains_at(hit, pager->search); hit++)
        ;
    if (*hit == '\0' || (size_t)(hit - line) >= sizeof(head) || needle >= sizeof(head))
        return;
    memcpy(head, line, (size_t)(hit - line));
    head[hit - line] = '\0';
    x = 1 + pnl_text_width(head);
    memcpy(head, hit, needle);
    head[needle] = '\0';
    pnl_canvas_text(canvas, x, y, PNL_REVERSE, head);
}

static void pnl_pager_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_pager *pager = ctx;
    int height = pnl_canvas_height(canvas);
    size_t i;

    (void)app;
    pager->page = height > 0 ? (size_t)height : 1;
    if (pager->top > pnl_pager_last_top(pager))
        pager->top = pnl_pager_last_top(pager);
    for (i = 0; i < pager->page && pager->top + i < pager->lines_len; i++)
        pnl_pager_draw_line(pager, canvas, (int)i, pager->top + i);
    pnl_pager_status(pager);
}

static void pnl_pager_find(struct pnl_pager *pager, size_t from, int forward)
{
    size_t count;

    pager->has_match = 0;
    if (pager->search[0] == '\0' || pager->lines_len == 0)
        return;
    for (count = 0; count < pager->lines_len; count++) {
        size_t index = forward ? (from + count) % pager->lines_len
                               : (from + pager->lines_len - count % pager->lines_len) % pager->lines_len;

        if (!pnl_text_contains(pnl_pager_line(pager, index), pager->search))
            continue;
        pager->match = index;
        pager->has_match = 1;
        if (index < pager->top || index >= pager->top + pager->page)
            pager->top = index > pager->page / 3 ? index - pager->page / 3 : 0;
        return;
    }
}

static int pnl_pager_search_key(struct pnl_pager *pager, int key)
{
    if (key == PNL_KEY_ENTER || key == PNL_KEY_ESCAPE) {
        pager->searching = 0;
        if (key == PNL_KEY_ESCAPE)
            pager->search[0] = '\0';
        pnl_pager_find(pager, pager->top, 1);
        return 1;
    }
    pnl_edit_key(&pager->edit, key);
    return 1;
}

static void pnl_pager_scroll(struct pnl_pager *pager, int key)
{
    size_t last = pnl_pager_last_top(pager);
    size_t step = key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN || key == ' ' ? pager->page : 1;

    if (key == PNL_KEY_HOME || key == 'g')
        pager->top = 0;
    else if (key == PNL_KEY_END || key == 'G')
        pager->top = last;
    else if (key == PNL_KEY_UP || key == 'k' || key == PNL_KEY_PAGE_UP)
        pager->top = pager->top > step ? pager->top - step : 0;
    else
        pager->top = pager->top + step < last ? pager->top + step : last;
}

static int pnl_pager_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_pager *pager = ctx;

    (void)app;
    if (pager->searching)
        return pnl_pager_search_key(pager, key);
    if (key == '/') {
        pager->searching = 1;
        pager->search[0] = '\0';
        pnl_edit_init(&pager->edit, pager->search, sizeof(pager->search));
        return 1;
    }
    if (key == 'n' || key == 'N') {
        size_t from = pager->has_match ? pager->match : pager->top;

        pnl_pager_find(pager, key == 'n' ? from + 1 : from + pager->lines_len - 1, key == 'n');
        return 1;
    }
    if (key == 'q' || key == PNL_KEY_ESCAPE)
        return 0;
    if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == 'j' || key == 'k' || key == ' ' || key == 'g' ||
        key == 'G' || (key >= PNL_KEY_PAGE_UP && key <= PNL_KEY_END))
        pnl_pager_scroll(pager, key);
    return 1;
}

static void pnl_pager_drop(pnl_app_t *app, void *ctx)
{
    struct pnl_pager *pager = ctx;

    pnl_mem_free(&app->mem, pager->text);
    pnl_mem_free(&app->mem, pager->lines);
    pnl_mem_free(&app->mem, pager);
}

static int pnl_pager_cli(const char *text, size_t text_len)
{
    fwrite(text, 1, text_len, stdout);
    if (text_len > 0 && text[text_len - 1] != '\n')
        putchar('\n');
    fflush(stdout);
    return PNL_OK;
}

static struct pnl_pager *pnl_pager_create(pnl_app_t *app, const char *text, size_t text_len)
{
    struct pnl_pager *pager = pnl_mem_alloc(&app->mem, sizeof(*pager));

    if (pager == NULL)
        return NULL;
    memset(pager, 0, sizeof(*pager));
    pager->text = pnl_mem_alloc(&app->mem, pnl_pager_expanded_len(text, text_len));
    if (pager->text != NULL) {
        pager->lines_len = pnl_pager_copy(pager, text, text_len);
        pager->lines = pnl_mem_alloc(&app->mem, pager->lines_len * sizeof(*pager->lines));
    }
    if (pager->text == NULL || pager->lines == NULL) {
        pnl_pager_drop(app, pager);
        return NULL;
    }
    pnl_pager_split(pager);
    pager->page = 1;
    pager->footer = pager->footer_text;
    pnl_pager_status(pager);
    return pager;
}

int pnl_pager(pnl_app_t *app, const char *title, const char *text, size_t text_len)
{
    struct pnl_pager *pager;
    struct pnl_view view;
    int status;

    if (app == NULL || title == NULL || (text == NULL && text_len > 0) || (!app->running && !app->cli))
        return PNL_ERR_ARG;
    if (app->cli)
        return pnl_pager_cli(text, text_len);
    pager = pnl_pager_create(app, text ? text : "", text_len);
    if (pager == NULL)
        return PNL_ERR_NOMEM;
    memset(&view, 0, sizeof(view));
    view.footer_ref = &pager->footer;
    view.draw_fn = pnl_pager_draw;
    view.key_fn = pnl_pager_key;
    view.drop_fn = pnl_pager_drop;
    view.ctx = pager;
    view.title = pnl_mem_copy(&app->mem, title);
    status = view.title == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &view);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, view.title);
        pnl_pager_drop(app, pager);
    }
    return status;
}
