#include "pnl_internal.h"

#include <stdlib.h>
#include <string.h>

#define PNL_CLEAR "\x1b[2J\x1b[H"
#define PNL_BOLD "\x1b[1m"
#define PNL_GRAY "\x1b[90m"
#define PNL_RESET "\x1b[0m"
#define PNL_HIDE_CURSOR "\x1b[?25l"
#define PNL_SHOW_CURSOR "\x1b[?25h"
#define PNL_CTRL_C 3
#define PNL_QUIT_LABEL "Quit"
#define PNL_MENU_FOOTER "up/down: move  enter: choose  q: quit"
#define PNL_RESULT_FOOTER "any key: back"

struct pnl_action {
    char *label;
    pnl_action_fn fn;
    void *ctx;
};

struct pnl_line {
    int mark;
    char *text;
};

struct pnl_app {
    pnl_alloc_fn alloc_fn;
    pnl_free_fn free_fn;
    void *alloc_ctx;
    char *title;
    struct pnl_action *actions;
    size_t actions_len;
    size_t actions_cap;
    pnl_status_fn status_fn;
    void *status_ctx;
    int refresh_ms;
    struct pnl_line *lines;
    size_t lines_len;
    size_t lines_cap;
    int in_status;
    int in_action;
    int result_set;
    int result_mark;
    char *result_text;
    char *out;
    size_t out_len;
    size_t out_cap;
    size_t selected;
    struct pnl_term term;
};

static void *pnl_default_alloc(void *ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

static void pnl_default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

static char *pnl_copy(pnl_app_t *app, const char *text)
{
    size_t len = strlen(text) + 1;
    char *copy = app->alloc_fn(app->alloc_ctx, len);

    if (copy != NULL)
        memcpy(copy, text, len);
    return copy;
}

static int pnl_grow(pnl_app_t *app, void **items, size_t *cap, size_t needed, size_t item_size)
{
    size_t new_cap;
    void *grown;

    if (needed <= *cap)
        return PNL_OK;
    new_cap = *cap ? *cap : 8;
    while (new_cap < needed && new_cap <= (size_t)-1 / 2)
        new_cap *= 2;
    if (new_cap < needed || new_cap > (size_t)-1 / item_size)
        return PNL_ERR_NOMEM;
    grown = app->alloc_fn(app->alloc_ctx, new_cap * item_size);
    if (grown == NULL)
        return PNL_ERR_NOMEM;
    if (*items != NULL) {
        memcpy(grown, *items, *cap * item_size);
        app->free_fn(app->alloc_ctx, *items);
    }
    *items = grown;
    *cap = new_cap;
    return PNL_OK;
}

int pnl_app_create(const char *title, pnl_alloc_fn alloc_fn, pnl_free_fn free_fn, void *alloc_ctx,
                   pnl_app_t **out_app)
{
    pnl_app_t *app;

    if (out_app == NULL || title == NULL || (alloc_fn == NULL) != (free_fn == NULL))
        return PNL_ERR_ARG;
    *out_app = NULL;
    if (alloc_fn == NULL) {
        alloc_fn = pnl_default_alloc;
        free_fn = pnl_default_free;
    }
    app = alloc_fn(alloc_ctx, sizeof(*app));
    if (app == NULL)
        return PNL_ERR_NOMEM;
    memset(app, 0, sizeof(*app));
    app->alloc_fn = alloc_fn;
    app->free_fn = free_fn;
    app->alloc_ctx = alloc_ctx;
    app->title = pnl_copy(app, title);
    if (app->title == NULL) {
        free_fn(alloc_ctx, app);
        return PNL_ERR_NOMEM;
    }
    *out_app = app;
    return PNL_OK;
}

static void pnl_clear_lines(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->lines_len; i++)
        app->free_fn(app->alloc_ctx, app->lines[i].text);
    app->lines_len = 0;
}

static void pnl_clear_result(pnl_app_t *app)
{
    if (app->result_text != NULL)
        app->free_fn(app->alloc_ctx, app->result_text);
    app->result_text = NULL;
    app->result_set = 0;
}

void pnl_app_destroy(pnl_app_t *app)
{
    size_t i;

    if (app == NULL)
        return;
    pnl_term_leave(&app->term, app->free_fn, app->alloc_ctx);
    for (i = 0; i < app->actions_len; i++)
        app->free_fn(app->alloc_ctx, app->actions[i].label);
    pnl_clear_lines(app);
    pnl_clear_result(app);
    if (app->actions != NULL)
        app->free_fn(app->alloc_ctx, app->actions);
    if (app->lines != NULL)
        app->free_fn(app->alloc_ctx, app->lines);
    if (app->out != NULL)
        app->free_fn(app->alloc_ctx, app->out);
    app->free_fn(app->alloc_ctx, app->title);
    app->free_fn(app->alloc_ctx, app);
}

int pnl_app_set_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx, int refresh_ms)
{
    if (app == NULL || refresh_ms < 0)
        return PNL_ERR_ARG;
    app->status_fn = status_fn;
    app->status_ctx = status_ctx;
    app->refresh_ms = refresh_ms;
    return PNL_OK;
}

int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx)
{
    struct pnl_action *action;
    int status;

    if (app == NULL || label == NULL || action_fn == NULL)
        return PNL_ERR_ARG;
    status = pnl_grow(app, (void **)&app->actions, &app->actions_cap, app->actions_len + 1, sizeof(*action));
    if (status != PNL_OK)
        return status;
    action = &app->actions[app->actions_len];
    action->label = pnl_copy(app, label);
    if (action->label == NULL)
        return PNL_ERR_NOMEM;
    action->fn = action_fn;
    action->ctx = action_ctx;
    app->actions_len++;
    return PNL_OK;
}

static int pnl_valid_mark(int mark)
{
    return mark == PNL_MARK_OK || mark == PNL_MARK_WAIT || mark == PNL_MARK_FAIL;
}

int pnl_status_line(pnl_app_t *app, int mark, const char *text)
{
    struct pnl_line *line;
    int status;

    if (app == NULL || text == NULL || !app->in_status || !pnl_valid_mark(mark))
        return PNL_ERR_ARG;
    status = pnl_grow(app, (void **)&app->lines, &app->lines_cap, app->lines_len + 1, sizeof(*line));
    if (status != PNL_OK)
        return status;
    line = &app->lines[app->lines_len];
    line->text = pnl_copy(app, text);
    if (line->text == NULL)
        return PNL_ERR_NOMEM;
    line->mark = mark;
    app->lines_len++;
    return PNL_OK;
}

int pnl_result(pnl_app_t *app, int mark, const char *text)
{
    char *copy;

    if (app == NULL || text == NULL || !app->in_action || !pnl_valid_mark(mark))
        return PNL_ERR_ARG;
    copy = pnl_copy(app, text);
    if (copy == NULL)
        return PNL_ERR_NOMEM;
    pnl_clear_result(app);
    app->result_text = copy;
    app->result_mark = mark;
    app->result_set = 1;
    return PNL_OK;
}

static int pnl_put(pnl_app_t *app, const char *text)
{
    size_t len = strlen(text);
    int status = pnl_grow(app, (void **)&app->out, &app->out_cap, app->out_len + len, 1);

    if (status != PNL_OK)
        return status;
    memcpy(app->out + app->out_len, text, len);
    app->out_len += len;
    return PNL_OK;
}

static int pnl_put_all(pnl_app_t *app, const char *const *parts, size_t parts_len)
{
    size_t i;
    int status = PNL_OK;

    for (i = 0; i < parts_len && status == PNL_OK; i++)
        status = pnl_put(app, parts[i]);
    return status;
}

static const char *pnl_mark_text(int mark)
{
    if (mark == PNL_MARK_OK)
        return "\x1b[32mok" PNL_RESET;
    if (mark == PNL_MARK_WAIT)
        return "\x1b[33m.." PNL_RESET;
    return "\x1b[31m!!" PNL_RESET;
}

static int pnl_put_line(pnl_app_t *app, int mark, const char *text)
{
    const char *parts[] = {"  ", pnl_mark_text(mark), "  ", text, "\r\n"};

    return pnl_put_all(app, parts, sizeof(parts) / sizeof(parts[0]));
}

static int pnl_put_header(pnl_app_t *app)
{
    const char *parts[] = {PNL_CLEAR PNL_BOLD, app->title, PNL_RESET "\r\n\r\n"};

    return pnl_put_all(app, parts, sizeof(parts) / sizeof(parts[0]));
}

static int pnl_put_footer(pnl_app_t *app, const char *footer)
{
    const char *parts[] = {"\r\n" PNL_GRAY, footer, PNL_RESET "\r\n"};

    return pnl_put_all(app, parts, sizeof(parts) / sizeof(parts[0]));
}

static int pnl_put_item(pnl_app_t *app, size_t index)
{
    const char *label = index < app->actions_len ? app->actions[index].label : PNL_QUIT_LABEL;
    const char *selected[] = {"  " PNL_BOLD "> ", label, PNL_RESET "\r\n"};
    const char *plain[] = {"    ", label, "\r\n"};

    if (index == app->selected)
        return pnl_put_all(app, selected, 3);
    return pnl_put_all(app, plain, 3);
}

static int pnl_flush(pnl_app_t *app)
{
    int status = pnl_term_write(app->out, app->out_len);

    app->out_len = 0;
    return status;
}

static int pnl_draw_menu(pnl_app_t *app)
{
    size_t i;
    int status = pnl_put_header(app);

    for (i = 0; i < app->lines_len && status == PNL_OK; i++)
        status = pnl_put_line(app, app->lines[i].mark, app->lines[i].text);
    if (status == PNL_OK && app->lines_len > 0)
        status = pnl_put(app, "\r\n");
    for (i = 0; i <= app->actions_len && status == PNL_OK; i++)
        status = pnl_put_item(app, i);
    if (status == PNL_OK)
        status = pnl_put_footer(app, PNL_MENU_FOOTER);
    if (status != PNL_OK) {
        app->out_len = 0;
        return status;
    }
    return pnl_flush(app);
}

static int pnl_draw_result(pnl_app_t *app)
{
    int status = pnl_put_header(app);

    if (status == PNL_OK)
        status = pnl_put_line(app, app->result_mark, app->result_text);
    if (status == PNL_OK)
        status = pnl_put_footer(app, PNL_RESULT_FOOTER);
    if (status != PNL_OK) {
        app->out_len = 0;
        return status;
    }
    return pnl_flush(app);
}

static void pnl_refresh(pnl_app_t *app)
{
    pnl_clear_lines(app);
    if (app->status_fn == NULL)
        return;
    app->in_status = 1;
    app->status_fn(app->status_ctx, app);
    app->in_status = 0;
}

static int pnl_wait_any_key(pnl_app_t *app)
{
    int key = PNL_KEY_NONE;
    int status = PNL_OK;

    while (status == PNL_OK && key == PNL_KEY_NONE)
        status = pnl_term_read_key(&app->term, -1, &key);
    return status;
}

static int pnl_run_action(pnl_app_t *app, struct pnl_action *action)
{
    int status;

    pnl_term_write(PNL_CLEAR PNL_SHOW_CURSOR, sizeof(PNL_CLEAR PNL_SHOW_CURSOR) - 1);
    pnl_term_leave(&app->term, NULL, NULL);
    pnl_clear_result(app);
    app->in_action = 1;
    action->fn(action->ctx, app);
    app->in_action = 0;
    status = pnl_term_enter(&app->term, app->alloc_fn, app->alloc_ctx);
    pnl_term_write(PNL_HIDE_CURSOR, sizeof(PNL_HIDE_CURSOR) - 1);
    if (status != PNL_OK || !app->result_set)
        return status;
    status = pnl_draw_result(app);
    if (status == PNL_OK)
        status = pnl_wait_any_key(app);
    pnl_clear_result(app);
    return status;
}

static int pnl_is_quit(int key)
{
    return key == 'q' || key == PNL_CTRL_C || key == PNL_KEY_ESCAPE;
}

static int pnl_handle_key(pnl_app_t *app, int key, int *out_done)
{
    size_t count = app->actions_len + 1;

    if (key == PNL_KEY_UP || key == 'k')
        app->selected = (app->selected + count - 1) % count;
    else if (key == PNL_KEY_DOWN || key == 'j')
        app->selected = (app->selected + 1) % count;
    else if (pnl_is_quit(key) || (key == PNL_KEY_ENTER && app->selected == app->actions_len))
        *out_done = 1;
    else if (key == PNL_KEY_ENTER)
        return pnl_run_action(app, &app->actions[app->selected]);
    return PNL_OK;
}

static int pnl_loop(pnl_app_t *app)
{
    int done = 0;
    int status = PNL_OK;
    int timeout = app->status_fn != NULL && app->refresh_ms > 0 ? app->refresh_ms : -1;

    while (!done && status == PNL_OK) {
        int key = PNL_KEY_NONE;

        pnl_refresh(app);
        status = pnl_draw_menu(app);
        if (status == PNL_OK)
            status = pnl_term_read_key(&app->term, timeout, &key);
        if (status == PNL_OK && key != PNL_KEY_NONE)
            status = pnl_handle_key(app, key, &done);
    }
    return status;
}

int pnl_app_run(pnl_app_t *app)
{
    int status;

    if (app == NULL)
        return PNL_ERR_ARG;
    if (!pnl_term_available())
        return PNL_ERR_TTY;
    status = pnl_term_enter(&app->term, app->alloc_fn, app->alloc_ctx);
    if (status != PNL_OK)
        return status;
    app->selected = 0;
    pnl_term_write(PNL_HIDE_CURSOR, sizeof(PNL_HIDE_CURSOR) - 1);
    status = pnl_loop(app);
    pnl_term_write(PNL_CLEAR PNL_SHOW_CURSOR, sizeof(PNL_CLEAR PNL_SHOW_CURSOR) - 1);
    pnl_term_leave(&app->term, app->free_fn, app->alloc_ctx);
    pnl_clear_lines(app);
    return status;
}

const char *pnl_strerror(int status)
{
    switch (status) {
    case PNL_OK:
        return "ok";
    case PNL_ERR_ARG:
        return "invalid argument";
    case PNL_ERR_NOMEM:
        return "out of memory";
    case PNL_ERR_TTY:
        return "no interactive terminal";
    case PNL_ERR_IO:
        return "terminal read or write failed";
    default:
        return "unknown status";
    }
}
