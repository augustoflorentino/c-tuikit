#include "pnl_internal.h"

#include <string.h>

#define PNL_QUIT_LABEL "Quit"
#define PNL_RESULT_FOOTER "any key: back"
#define PNL_SELECTED_STYLE PNL_BOLD

struct pnl_result_view {
    int mark;
    char *text;
};

void pnl_menu_clear_lines(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->lines_len; i++)
        pnl_mem_free(&app->mem, app->lines[i].text);
    app->lines_len = 0;
}

int pnl_valid_mark(int mark)
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

void pnl_draw_mark(pnl_canvas_t *canvas, int y, int mark, const char *text)
{
    static const char *const marks[] = {"ok", "..", "!!"};
    static const int colors[] = {PNL_COLOR_GREEN, PNL_COLOR_YELLOW, PNL_COLOR_RED};

    pnl_canvas_text(canvas, 2, y, PNL_FG(colors[mark]), marks[mark]);
    pnl_canvas_text(canvas, 6, y, 0, text);
}

void pnl_menu_refresh(pnl_app_t *app)
{
    pnl_menu_clear_lines(app);
    if (app->status_fn == NULL)
        return;
    app->in_status = 1;
    app->status_fn(app->status_ctx, app);
    app->in_status = 0;
}

static void pnl_draw_item(pnl_app_t *app, pnl_canvas_t *canvas, int y, size_t index)
{
    const char *label = index < app->actions_len ? app->actions[index].label : PNL_QUIT_LABEL;

    if (index != app->selected) {
        pnl_canvas_text(canvas, 4, y, 0, label);
        return;
    }
    pnl_canvas_text(canvas, 2, y, PNL_SELECTED_STYLE, "> ");
    pnl_canvas_text(canvas, 4, y, PNL_SELECTED_STYLE, label);
}

static void pnl_menu_scroll(pnl_app_t *app, size_t rows)
{
    if (rows == 0)
        rows = 1;
    if (app->selected < app->menu_top)
        app->menu_top = app->selected;
    if (app->selected >= app->menu_top + rows)
        app->menu_top = app->selected - rows + 1;
    if (app->menu_top + rows > app->actions_len + 1)
        app->menu_top = app->actions_len + 1 > rows ? app->actions_len + 1 - rows : 0;
}

void pnl_menu_draw(pnl_app_t *app, pnl_canvas_t *canvas)
{
    size_t i;
    int y = 0;
    int rows;

    pnl_menu_refresh(app);
    for (i = 0; i < app->lines_len; i++)
        pnl_draw_mark(canvas, y++, app->lines[i].mark, app->lines[i].text);
    if (app->lines_len > 0)
        y++;
    rows = pnl_canvas_height(canvas) - y;
    pnl_menu_scroll(app, rows > 0 ? (size_t)rows : 0);
    for (i = app->menu_top; i <= app->actions_len && y < pnl_canvas_height(canvas); i++)
        pnl_draw_item(app, canvas, y++, i);
    if (app->menu_top > 0)
        pnl_canvas_text(canvas, 0, y - rows, PNL_FG(PNL_COLOR_GRAY), "^");
    if (i <= app->actions_len)
        pnl_canvas_text(canvas, 0, y - 1, PNL_FG(PNL_COLOR_GRAY), "v");
}

int pnl_menu_key(pnl_app_t *app, int key)
{
    size_t count = app->actions_len + 1;

    if (key == PNL_KEY_UP || key == 'k')
        app->selected = (app->selected + count - 1) % count;
    else if (key == PNL_KEY_DOWN || key == 'j')
        app->selected = (app->selected + 1) % count;
    else if (key == 'q' || key == PNL_KEY_ESCAPE || (key == PNL_KEY_ENTER && app->selected == app->actions_len))
        app->quit = 1;
    else if (key == PNL_KEY_ENTER)
        app->actions[app->selected].fn(app->actions[app->selected].ctx, app);
    else
        return 0;
    return 1;
}

static void pnl_result_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_result_view *view = ctx;

    (void)app;
    pnl_draw_mark(canvas, 0, view->mark, view->text);
}

static int pnl_result_key(void *ctx, pnl_app_t *app, int key)
{
    (void)ctx;
    (void)key;
    pnl_view_pop(app);
    return 1;
}

static void pnl_result_drop(pnl_app_t *app, void *ctx)
{
    struct pnl_result_view *view = ctx;

    pnl_mem_free(&app->mem, view->text);
    pnl_mem_free(&app->mem, view);
}

int pnl_result(pnl_app_t *app, int mark, const char *text)
{
    struct pnl_result_view *result;
    struct pnl_view view;
    int status;

    if (app == NULL || text == NULL || !pnl_valid_mark(mark) || (!app->running && !app->cli))
        return PNL_ERR_ARG;
    if (app->cli)
        return pnl_cli_result(app, mark, text);
    result = pnl_mem_alloc(&app->mem, sizeof(*result));
    if (result == NULL)
        return PNL_ERR_NOMEM;
    result->mark = mark;
    result->text = pnl_mem_copy(&app->mem, text);
    memset(&view, 0, sizeof(view));
    view.footer = PNL_RESULT_FOOTER;
    view.draw_fn = pnl_result_draw;
    view.key_fn = pnl_result_key;
    view.drop_fn = pnl_result_drop;
    view.ctx = result;
    status = result->text == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &view);
    if (status != PNL_OK)
        pnl_result_drop(app, result);
    return status;
}
