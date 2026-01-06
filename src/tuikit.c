#include "pnl_internal.h"

#include <stdlib.h>
#include <string.h>

#define PNL_CTRL_C 3
#define PNL_QUIT_LABEL "Quit"
#define PNL_MENU_FOOTER "up/down: move  enter: choose  q: quit"
#define PNL_RESULT_FOOTER "any key: back"
#define PNL_STYLE_TITLE PNL_PACK(PNL_INK_DEFAULT, 0, PNL_ATTR_BOLD)
#define PNL_STYLE_FOOTER PNL_PACK(PNL_INK_GRAY, 0, 0)
#define PNL_STYLE_PLAIN 0u

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
    struct pnl_mem mem;
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
    size_t selected;
    struct pnl_term term;
    struct pnl_screen screen;
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

void *pnl_mem_alloc(const struct pnl_mem *mem, size_t size)
{
    return mem->alloc_fn(mem->ctx, size);
}

void pnl_mem_free(const struct pnl_mem *mem, void *ptr)
{
    if (ptr != NULL)
        mem->free_fn(mem->ctx, ptr);
}

char *pnl_mem_copy(const struct pnl_mem *mem, const char *text)
{
    size_t len = strlen(text) + 1;
    char *copy = pnl_mem_alloc(mem, len);

    if (copy != NULL)
        memcpy(copy, text, len);
    return copy;
}

int pnl_mem_grow(const struct pnl_mem *mem, void **items, size_t *cap, size_t needed, size_t item_size)
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
    grown = pnl_mem_alloc(mem, new_cap * item_size);
    if (grown == NULL)
        return PNL_ERR_NOMEM;
    if (*items != NULL) {
        memcpy(grown, *items, *cap * item_size);
        pnl_mem_free(mem, *items);
    }
    *items = grown;
    *cap = new_cap;
    return PNL_OK;
}

int pnl_app_create(const char *title, pnl_alloc_fn alloc_fn, pnl_free_fn free_fn, void *alloc_ctx,
                   pnl_app_t **out_app)
{
    struct pnl_mem mem;
    pnl_app_t *app;

    if (out_app == NULL || title == NULL || (alloc_fn == NULL) != (free_fn == NULL))
        return PNL_ERR_ARG;
    *out_app = NULL;
    mem.alloc_fn = alloc_fn ? alloc_fn : pnl_default_alloc;
    mem.free_fn = free_fn ? free_fn : pnl_default_free;
    mem.ctx = alloc_ctx;
    app = pnl_mem_alloc(&mem, sizeof(*app));
    if (app == NULL)
        return PNL_ERR_NOMEM;
    memset(app, 0, sizeof(*app));
    app->mem = mem;
    app->title = pnl_mem_copy(&mem, title);
    if (app->title == NULL) {
        pnl_mem_free(&mem, app);
        return PNL_ERR_NOMEM;
    }
    *out_app = app;
    return PNL_OK;
}

static void pnl_clear_lines(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->lines_len; i++)
        pnl_mem_free(&app->mem, app->lines[i].text);
    app->lines_len = 0;
}

static void pnl_clear_result(pnl_app_t *app)
{
    pnl_mem_free(&app->mem, app->result_text);
    app->result_text = NULL;
    app->result_set = 0;
}

void pnl_app_destroy(pnl_app_t *app)
{
    size_t i;

    if (app == NULL)
        return;
    pnl_term_leave(&app->term);
    pnl_screen_free(&app->screen, &app->mem);
    for (i = 0; i < app->actions_len; i++)
        pnl_mem_free(&app->mem, app->actions[i].label);
    pnl_clear_lines(app);
    pnl_clear_result(app);
    pnl_mem_free(&app->mem, app->actions);
    pnl_mem_free(&app->mem, app->lines);
    pnl_mem_free(&app->mem, app->title);
    pnl_mem_free(&app->mem, app);
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
    status = pnl_mem_grow(&app->mem, (void **)&app->actions, &app->actions_cap, app->actions_len + 1,
                          sizeof(*action));
    if (status != PNL_OK)
        return status;
    action = &app->actions[app->actions_len];
    action->label = pnl_mem_copy(&app->mem, label);
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
    status = pnl_mem_grow(&app->mem, (void **)&app->lines, &app->lines_cap, app->lines_len + 1, sizeof(*line));
    if (status != PNL_OK)
        return status;
    line = &app->lines[app->lines_len];
    line->text = pnl_mem_copy(&app->mem, text);
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
    copy = pnl_mem_copy(&app->mem, text);
    if (copy == NULL)
        return PNL_ERR_NOMEM;
    pnl_clear_result(app);
    app->result_text = copy;
    app->result_mark = mark;
    app->result_set = 1;
    return PNL_OK;
}

static void pnl_draw_mark(pnl_app_t *app, int y, int mark, const char *text)
{
    static const char *const marks[] = {"ok", "..", "!!"};
    static const uint32_t inks[] = {PNL_INK_GREEN, PNL_INK_YELLOW, PNL_INK_RED};

    pnl_screen_text(&app->screen, 2, y, -1, PNL_PACK(inks[mark], 0, 0), marks[mark]);
    pnl_screen_text(&app->screen, 6, y, -1, PNL_STYLE_PLAIN, text);
}

static int pnl_draw_frame(pnl_app_t *app, const char *footer)
{
    int width;
    int height;
    int status;

    pnl_term_size(&width, &height);
    status = pnl_screen_resize(&app->screen, &app->mem, width, height);
    if (status != PNL_OK)
        return status;
    pnl_screen_clear(&app->screen);
    pnl_screen_text(&app->screen, 0, 0, -1, PNL_STYLE_TITLE, app->title);
    pnl_screen_text(&app->screen, 0, height - 1, -1, PNL_STYLE_FOOTER, footer);
    return PNL_OK;
}

static void pnl_draw_item(pnl_app_t *app, int y, size_t index)
{
    const char *label = index < app->actions_len ? app->actions[index].label : PNL_QUIT_LABEL;

    if (index != app->selected) {
        pnl_screen_text(&app->screen, 4, y, -1, PNL_STYLE_PLAIN, label);
        return;
    }
    pnl_screen_text(&app->screen, 2, y, -1, PNL_PACK(PNL_INK_DEFAULT, 0, PNL_ATTR_BOLD), "> ");
    pnl_screen_text(&app->screen, 4, y, -1, PNL_PACK(PNL_INK_DEFAULT, 0, PNL_ATTR_BOLD), label);
}

static int pnl_draw_menu(pnl_app_t *app)
{
    size_t i;
    int y = 2;
    int status = pnl_draw_frame(app, PNL_MENU_FOOTER);

    if (status != PNL_OK)
        return status;
    for (i = 0; i < app->lines_len; i++)
        pnl_draw_mark(app, y++, app->lines[i].mark, app->lines[i].text);
    if (app->lines_len > 0)
        y++;
    for (i = 0; i <= app->actions_len; i++)
        pnl_draw_item(app, y++, i);
    return pnl_screen_render(&app->screen, &app->mem);
}

static int pnl_draw_result(pnl_app_t *app)
{
    int status = pnl_draw_frame(app, PNL_RESULT_FOOTER);

    if (status != PNL_OK)
        return status;
    pnl_draw_mark(app, 2, app->result_mark, app->result_text);
    return pnl_screen_render(&app->screen, &app->mem);
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

static int pnl_read_key(pnl_app_t *app, int timeout_ms, int *out_key)
{
    int status = pnl_term_read_key(&app->term, timeout_ms, out_key);

    if (status == PNL_OK && *out_key == PNL_KEY_SUSPEND) {
        pnl_term_suspend(&app->term);
        app->screen.front_valid = 0;
        *out_key = PNL_KEY_NONE;
    }
    if (pnl_term_resized())
        app->screen.front_valid = 0;
    return status;
}

static int pnl_wait_any_key(pnl_app_t *app)
{
    int key = PNL_KEY_NONE;
    int status = PNL_OK;

    while (status == PNL_OK && key == PNL_KEY_NONE) {
        status = pnl_read_key(app, -1, &key);
        if (status == PNL_OK && !app->screen.front_valid)
            status = pnl_draw_result(app);
    }
    return status;
}

static int pnl_run_action(pnl_app_t *app, struct pnl_action *action)
{
    int status;

    pnl_term_leave(&app->term);
    pnl_clear_result(app);
    app->in_action = 1;
    action->fn(action->ctx, app);
    app->in_action = 0;
    status = pnl_term_enter(&app->term);
    app->screen.front_valid = 0;
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
            status = pnl_read_key(app, timeout, &key);
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
    status = pnl_term_enter(&app->term);
    if (status != PNL_OK)
        return status;
    app->selected = 0;
    app->screen.front_valid = 0;
    status = pnl_loop(app);
    pnl_term_leave(&app->term);
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
