#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_CONFIRM_FOOTER "y/n  left/right: choose  enter: answer  esc: no"
#define PNL_INPUT_FOOTER "enter: accept  esc: cancel"
#define PNL_INPUT_HISTORY_FOOTER "up/down: history  enter: accept  esc: cancel"
#define PNL_PICK_FOOTER "type to filter  up/down: move  enter: choose  esc: cancel"
#define PNL_FILTER_MAX 128
#define PNL_ANSWER_MAX 16
#define PNL_HISTORY_MAX 20
#define PNL_HISTORY_TEXT 4096

struct pnl_confirm_view {
    const char *question;
    int yes;
    int answered;
};

struct pnl_input_view {
    const char *prompt;
    struct pnl_edit edit;
    const char *history;
    const char *draft;
    size_t recalled;
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

static int pnl_confirm_key(void *ctx, pnl_app_t *app, int key);

static int pnl_prompt_ready(const pnl_app_t *app)
{
    return app->running || app->cli;
}

static const int pnl_typing = 1;

static int pnl_prompt_modal(pnl_app_t *app, const char *title, const char *footer, int width, int height,
                            pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *ctx)
{
    struct pnl_view view;

    memset(&view, 0, sizeof(view));
    view.typing_ref = key_fn == pnl_confirm_key ? NULL : &pnl_typing;
    view.dialog_width = width < 24 ? 24 : width;
    view.dialog_height = height;
    view.footer = footer;
    view.draw_fn = draw_fn;
    view.key_fn = key_fn;
    view.ctx = ctx;
    view.title = pnl_mem_copy(&app->mem, title);
    if (view.title == NULL)
        return PNL_ERR_NOMEM;
    return pnl_view_modal(app, &view);
}

static void pnl_confirm_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_confirm_view *view = ctx;

    (void)app;
    pnl_canvas_text(canvas, 0, 0, PNL_BOLD, view->question);
    pnl_canvas_text(canvas, 0, 2, view->yes ? PNL_REVERSE : 0, " yes ");
    pnl_canvas_text(canvas, 7, 2, view->yes ? 0 : PNL_REVERSE, " no ");
}

static int pnl_confirm_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_confirm_view *view = ctx;
    int x;
    int y;

    if (key == PNL_KEY_CLICK) {
        pnl_mouse_position(app, &x, &y);
        if (y != 2 || x < 0 || x > 10 || x == 5 || x == 6)
            return 1;
        view->yes = x < 5;
        key = PNL_KEY_ENTER;
    }
    if (key == PNL_KEY_LEFT || key == PNL_KEY_RIGHT || key == PNL_KEY_TAB || key == 'h' || key == 'l') {
        view->yes = !view->yes;
        return 1;
    }
    if (key == 'y' || key == 'Y' || key == 'n' || key == 'N' || key == PNL_KEY_ESCAPE)
        view->yes = key == 'y' || key == 'Y';
    else if (key != PNL_KEY_ENTER) {
        if (key >= ' ' && key < 0x7f)
            pnl_view_shake(app);
        return 1;
    }
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
        app->cli_worst = PNL_MARK_FAIL;
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
    status = pnl_prompt_modal(app, "confirm", PNL_CONFIRM_FOOTER, pnl_text_width(question), 3, pnl_confirm_draw, pnl_confirm_key, &view);
    *out_yes = view.answered && view.yes;
    return status;
}

static void pnl_input_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_input_view *view = ctx;

    (void)app;
    pnl_canvas_text(canvas, 0, 0, PNL_BOLD, view->prompt);
    pnl_canvas_fill(canvas, 0, 2, pnl_canvas_width(canvas), 1, PNL_UNDERLINE);
    pnl_edit_draw(&view->edit, canvas, 0, 2, pnl_canvas_width(canvas), 1);
}

static const char *pnl_history_entry(const char *history, size_t index, size_t *out_len)
{
    const char *end;

    while (history != NULL && index-- > 0) {
        history = (strchr)(history, '\n');
        history = history != NULL ? history + 1 : NULL;
    }
    if (history == NULL || *history == '\0')
        return NULL;
    end = (strchr)(history, '\n');
    *out_len = end != NULL ? (size_t)(end - history) : strlen(history);
    return history;
}

static void pnl_input_recall(struct pnl_input_view *view, int key)
{
    size_t index = key == PNL_KEY_UP ? view->recalled + 1 : view->recalled - 1;
    const char *entry = view->draft;
    size_t len = strlen(view->draft);

    if (key == PNL_KEY_DOWN && view->recalled == 0)
        return;
    if (index > 0 && (entry = pnl_history_entry(view->history, index - 1, &len)) == NULL)
        return;
    view->recalled = index;
    if (len >= view->edit.cap) {
        len = view->edit.cap - 1;
        while (len > 0 && ((unsigned char)entry[len] & 0xc0) == 0x80)
            len--;
    }
    memcpy(view->edit.text, entry, len);
    view->edit.text[len] = '\0';
    pnl_edit_init(&view->edit, view->edit.text, view->edit.cap);
}

static void pnl_history_add(pnl_app_t *app, const char *prompt, const char *text)
{
    const char *old = pnl_memory_get(app, "input", prompt);
    char history[PNL_HISTORY_TEXT];
    size_t used = 0;
    size_t count = 1;
    size_t index;
    size_t len;
    const char *entry;

    if (text[0] == '\0' || (strchr)(text, '\n') != NULL || strlen(text) >= sizeof(history))
        return;
    used = (size_t)snprintf(history, sizeof(history), "%s", text);
    for (index = 0; count < PNL_HISTORY_MAX && (entry = pnl_history_entry(old, index, &len)) != NULL; index++) {
        if ((len == strlen(text) && memcmp(entry, text, len) == 0) || used + len + 2 > sizeof(history))
            continue;
        history[used++] = '\n';
        memcpy(history + used, entry, len);
        used += len;
        history[used] = '\0';
        count++;
    }
    pnl_memory_set(app, "input", prompt, history);
}

static int pnl_input_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_input_view *view = ctx;

    if (key == PNL_KEY_UP || key == PNL_KEY_DOWN) {
        pnl_input_recall(view, key);
        return 1;
    }
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
    view.draft = text;
    view.history = pnl_memory_get(app, "input", prompt);
    pnl_edit_init(&view.edit, work, text_len);
    status = pnl_prompt_modal(app, "input", view.history != NULL ? PNL_INPUT_HISTORY_FOOTER : PNL_INPUT_FOOTER, 48, 3,
                              pnl_input_draw, pnl_input_key, &view);
    if (view.ok) {
        memcpy(text, work, view.edit.len + 1);
        pnl_history_add(app, prompt, text);
    }
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
    pnl_canvas_text(canvas, 0, 0, PNL_FG(PNL_ROLE_MUTED), "filter:");
    pnl_edit_draw(&view->edit, canvas, 8, 0, pnl_canvas_width(canvas) - 8, 1);
    if (rows <= 0)
        return;
    if (view->selected < view->top)
        view->top = view->selected;
    if (view->selected >= view->top + (size_t)rows)
        view->top = view->selected - (size_t)rows + 1;
    for (i = view->top; i < view->shown_len && i < view->top + (size_t)rows; i++) {
        int y = 2 + (int)(i - view->top);
        int selected = i == view->selected;

        pnl_canvas_text(canvas, 0, y, selected ? PNL_BOLD : 0, selected ? "> " : "  ");
        pnl_canvas_text(canvas, 2, y, selected ? PNL_BOLD : 0, view->items[view->shown[i]]);
    }
    if (view->shown_len == 0)
        pnl_canvas_text(canvas, 2, 2, PNL_FG(PNL_ROLE_MUTED), "no match");
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
    int x;
    int y;

    if (key == PNL_KEY_WHEEL_UP || key == PNL_KEY_WHEEL_DOWN)
        key = key == PNL_KEY_WHEEL_UP ? PNL_KEY_UP : PNL_KEY_DOWN;
    if (key == PNL_KEY_CLICK) {
        pnl_mouse_position(app, &x, &y);
        if (y < 2 || view->top + (size_t)(y - 2) >= view->shown_len)
            return 1;
        view->selected = view->top + (size_t)(y - 2);
        key = PNL_KEY_ENTER;
    }
    if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN) {
        pnl_pick_move(view, key);
        return 1;
    }
    if (key == PNL_KEY_ESCAPE || (key == PNL_KEY_ENTER && view->shown_len > 0)) {
        view->ok = key == PNL_KEY_ENTER;
        pnl_view_pop(app);
        return 1;
    }
    if (key == PNL_KEY_ENTER)
        pnl_view_shake(app);
    else if (pnl_edit_key(&view->edit, key))
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
    for (i = 0; i < items_len; i++) {
        if (strcmp(items[i], line) == 0) {
            *out_index = i;
            *out_ok = 1;
            break;
        }
    }
    return PNL_OK;
}

static int pnl_pick_width(const char *const *items, size_t items_len)
{
    int width = 30;
    size_t i;

    for (i = 0; i < items_len; i++) {
        if (pnl_text_width(items[i]) + 2 > width)
            width = pnl_text_width(items[i]) + 2;
    }
    return width;
}

static void pnl_pick_recall(const pnl_app_t *app, const char *title, struct pnl_pick_view *view)
{
    const char *last = pnl_memory_get(app, "pick", title);
    size_t i;

    for (i = 0; last != NULL && i < view->items_len; i++) {
        if (strcmp(view->items[i], last) == 0) {
            view->selected = i;
            return;
        }
    }
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
    pnl_pick_recall(app, title, &view);
    status = pnl_prompt_modal(app, title, PNL_PICK_FOOTER, pnl_pick_width(items, items_len),
                              items_len + 2 < 16 ? (int)items_len + 2 : 16, pnl_pick_draw, pnl_pick_key, &view);
    if (view.ok) {
        *out_index = view.shown[view.selected];
        *out_ok = 1;
        pnl_memory_set(app, "pick", title, items[*out_index]);
    }
    pnl_mem_free(&app->mem, view.shown);
    return status;
}
