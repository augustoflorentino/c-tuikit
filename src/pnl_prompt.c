#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_CONFIRM_FOOTER "y/n  left/right: choose  enter: answer  esc: no"
#define PNL_INPUT_FOOTER "enter: accept  esc: cancel"
#define PNL_PICK_FOOTER "type to filter  up/down: move  enter: choose  esc: cancel"
#define PNL_FILTER_MAX 128
#define PNL_ANSWER_MAX 16

struct pnl_confirm_view {
    const char *question;
    int yes;
    int answered;
};

struct pnl_input_view {
    const char *prompt;
    struct pnl_edit edit;
    int ok;
};

struct pnl_pick_view {
    const char *const *items;
    size_t items_len;
    size_t *shown;
    size_t shown_len;
    size_t selected;
    size_t top;
    char filter[PNL_FILTER_MAX];
    struct pnl_edit edit;
    int ok;
};

static int pnl_prompt_ready(const pnl_app_t *app)
{
    return app->running || app->cli;
}

static int pnl_prompt_modal(pnl_app_t *app, const char *title, const char *footer, pnl_draw_fn draw_fn,
                            pnl_key_fn key_fn, void *ctx)
{
    struct pnl_view view;
    int status;

    memset(&view, 0, sizeof(view));
    view.footer = footer;
    view.draw_fn = draw_fn;
    view.key_fn = key_fn;
    view.ctx = ctx;
    view.title = pnl_mem_copy(&app->mem, title);
    if (view.title == NULL)
        return PNL_ERR_NOMEM;
    status = pnl_view_modal(app, &view);
    if (status == PNL_ERR_NOMEM || status == PNL_ERR_TTY)
        pnl_mem_free(&app->mem, view.title);
    return status;
}

static void pnl_confirm_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_confirm_view *view = ctx;

    (void)app;
    pnl_canvas_text(canvas, 2, 0, PNL_BOLD, view->question);
    pnl_canvas_text(canvas, 2, 2, view->yes ? PNL_REVERSE : 0, " yes ");
    pnl_canvas_text(canvas, 9, 2, view->yes ? 0 : PNL_REVERSE, " no ");
}

static int pnl_confirm_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_confirm_view *view = ctx;

    if (key == PNL_KEY_LEFT || key == PNL_KEY_RIGHT || key == '\t' || key == 'h' || key == 'l') {
        view->yes = !view->yes;
        return 1;
    }
    if (key == 'y' || key == 'Y' || key == 'n' || key == 'N' || key == PNL_KEY_ESCAPE)
        view->yes = key == 'y' || key == 'Y';
    else if (key != PNL_KEY_ENTER)
        return 1;
    view->answered = 1;
    pnl_view_pop(app);
    return 1;
}

static int pnl_confirm_cli(pnl_app_t *app, const char *question, int *out_yes)
{
    char answer[PNL_ANSWER_MAX];
    char prompt[512];

    *out_yes = app->cli_yes;
    if (app->cli_yes)
        return PNL_OK;
    if (!pnl_term_stdin_is_tty()) {
        fprintf(stderr, "%s not confirmed: pass --yes\n", question);
        return PNL_OK;
    }
    snprintf(prompt, sizeof(prompt), "%s [y/N] ", question);
    if (pnl_cli_read_line(prompt, answer, sizeof(answer)))
        *out_yes = answer[0] == 'y' || answer[0] == 'Y';
    return PNL_OK;
}

int pnl_confirm(pnl_app_t *app, const char *question, int *out_yes)
{
    struct pnl_confirm_view view;
    int status;

    if (app == NULL || question == NULL || out_yes == NULL || !pnl_prompt_ready(app))
        return PNL_ERR_ARG;
    *out_yes = 0;
    if (app->cli)
        return pnl_confirm_cli(app, question, out_yes);
    memset(&view, 0, sizeof(view));
    view.question = question;
    status = pnl_prompt_modal(app, "confirm", PNL_CONFIRM_FOOTER, pnl_confirm_draw, pnl_confirm_key, &view);
    *out_yes = view.answered && view.yes;
    return status;
}

static void pnl_input_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_input_view *view = ctx;

    (void)app;
    pnl_canvas_text(canvas, 2, 0, PNL_BOLD, view->prompt);
    pnl_canvas_fill(canvas, 2, 2, pnl_canvas_width(canvas) - 4, 1, PNL_UNDERLINE);
    pnl_edit_draw(&view->edit, canvas, 2, 2, pnl_canvas_width(canvas) - 4, 1);
}

static int pnl_input_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_input_view *view = ctx;

    if (key == PNL_KEY_ENTER || key == PNL_KEY_ESCAPE) {
        view->ok = key == PNL_KEY_ENTER;
        pnl_view_pop(app);
        return 1;
    }
    pnl_edit_key(&view->edit, key);
    return 1;
}

int pnl_input(pnl_app_t *app, const char *prompt, char *text, size_t text_len, int *out_ok)
{
    struct pnl_input_view view;
    char *work;
    int status;

    if (app == NULL || prompt == NULL || text == NULL || text_len == 0 || out_ok == NULL ||
        (memchr)(text, '\0', text_len) == NULL || !pnl_prompt_ready(app))
        return PNL_ERR_ARG;
    *out_ok = 0;
    if (app->cli) {
        char line[1024];

        snprintf(line, sizeof(line), "%s: ", prompt);
        *out_ok = pnl_cli_read_line(line, text, text_len);
        return PNL_OK;
    }
    work = pnl_mem_alloc(&app->mem, text_len);
    if (work == NULL)
        return PNL_ERR_NOMEM;
    memcpy(work, text, text_len);
    memset(&view, 0, sizeof(view));
    view.prompt = prompt;
    pnl_edit_init(&view.edit, work, text_len);
    status = pnl_prompt_modal(app, "input", PNL_INPUT_FOOTER, pnl_input_draw, pnl_input_key, &view);
    if (view.ok)
        memcpy(text, work, view.edit.len + 1);
    *out_ok = view.ok;
    pnl_mem_free(&app->mem, work);
    return status;
}

static int pnl_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int pnl_text_contains_at(const char *text, const char *needle)
{
    size_t i;

    for (i = 0; needle[i] != '\0'; i++) {
        if (pnl_lower((unsigned char)text[i]) != pnl_lower((unsigned char)needle[i]))
            return 0;
    }
    return 1;
}

int pnl_text_contains(const char *text, const char *needle)
{
    if (needle[0] == '\0')
        return 1;
    for (; *text != '\0'; text++) {
        if (pnl_text_contains_at(text, needle))
            return 1;
    }
    return 0;
}

static void pnl_pick_filter(struct pnl_pick_view *view)
{
    size_t i;

    view->shown_len = 0;
    for (i = 0; i < view->items_len; i++) {
        if (pnl_text_contains(view->items[i], view->filter))
            view->shown[view->shown_len++] = i;
    }
    view->selected = 0;
    view->top = 0;
}

static void pnl_pick_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_pick_view *view = ctx;
    int rows = pnl_canvas_height(canvas) - 2;
    size_t i;

    (void)app;
    pnl_canvas_text(canvas, 2, 0, PNL_FG(PNL_COLOR_GRAY), "filter:");
    pnl_edit_draw(&view->edit, canvas, 10, 0, pnl_canvas_width(canvas) - 12, 1);
    if (rows <= 0)
        return;
    if (view->selected < view->top)
        view->top = view->selected;
    if (view->selected >= view->top + (size_t)rows)
        view->top = view->selected - (size_t)rows + 1;
    for (i = view->top; i < view->shown_len && i < view->top + (size_t)rows; i++) {
        int y = 2 + (int)(i - view->top);
        int selected = i == view->selected;

        pnl_canvas_text(canvas, 2, y, selected ? PNL_BOLD : 0, selected ? "> " : "  ");
        pnl_canvas_text(canvas, 4, y, selected ? PNL_BOLD : 0, view->items[view->shown[i]]);
    }
    if (view->shown_len == 0)
        pnl_canvas_text(canvas, 4, 2, PNL_FG(PNL_COLOR_GRAY), "no match");
}

static void pnl_pick_move(struct pnl_pick_view *view, int key)
{
    size_t step = key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN ? 10 : 1;

    if (view->shown_len == 0)
        return;
    if (key == PNL_KEY_UP || key == PNL_KEY_PAGE_UP)
        view->selected = view->selected > step ? view->selected - step : 0;
    else
        view->selected = view->selected + step < view->shown_len ? view->selected + step : view->shown_len - 1;
}

static int pnl_pick_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_pick_view *view = ctx;

    if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN) {
        pnl_pick_move(view, key);
        return 1;
    }
    if (key == PNL_KEY_ESCAPE || (key == PNL_KEY_ENTER && view->shown_len > 0)) {
        view->ok = key == PNL_KEY_ENTER;
        pnl_view_pop(app);
        return 1;
    }
    if (pnl_edit_key(&view->edit, key))
        pnl_pick_filter(view);
    return 1;
}

static int pnl_pick_cli(const char *title, const char *const *items, size_t items_len, size_t *out_index,
                        int *out_ok)
{
    char line[1024];
    char *end;
    size_t i;

    if (pnl_term_stdin_is_tty()) {
        fprintf(stderr, "%s\n", title);
        for (i = 0; i < items_len; i++)
            fprintf(stderr, "  %zu) %s\n", i + 1, items[i]);
    }
    if (!pnl_cli_read_line("choice: ", line, sizeof(line)))
        return PNL_OK;
    i = (size_t)strtoul(line, &end, 10);
    if (end != line && *end == '\0' && i >= 1 && i <= items_len) {
        *out_index = i - 1;
        *out_ok = 1;
        return PNL_OK;
    }
    for (i = 0; i < items_len && !*out_ok; i++) {
        *out_ok = strcmp(items[i], line) == 0;
        *out_index = i;
    }
    return PNL_OK;
}

int pnl_pick(pnl_app_t *app, const char *title, const char *const *items, size_t items_len, size_t *out_index,
             int *out_ok)
{
    struct pnl_pick_view view;
    size_t i;
    int status;

    if (app == NULL || title == NULL || items == NULL || items_len == 0 || out_index == NULL || out_ok == NULL ||
        !pnl_prompt_ready(app))
        return PNL_ERR_ARG;
    for (i = 0; i < items_len; i++) {
        if (items[i] == NULL)
            return PNL_ERR_ARG;
    }
    *out_ok = 0;
    *out_index = 0;
    if (app->cli)
        return pnl_pick_cli(title, items, items_len, out_index, out_ok);
    memset(&view, 0, sizeof(view));
    view.items = items;
    view.items_len = items_len;
    view.shown = pnl_mem_alloc(&app->mem, items_len * sizeof(*view.shown));
    if (view.shown == NULL)
        return PNL_ERR_NOMEM;
    pnl_edit_init(&view.edit, view.filter, sizeof(view.filter));
    pnl_pick_filter(&view);
    status = pnl_prompt_modal(app, title, PNL_PICK_FOOTER, pnl_pick_draw, pnl_pick_key, &view);
    if (view.ok) {
        *out_index = view.shown[view.selected];
        *out_ok = 1;
    }
    pnl_mem_free(&app->mem, view.shown);
    return status;
}
