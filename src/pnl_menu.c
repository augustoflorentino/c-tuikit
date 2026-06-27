#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_QUIT_LABEL "Quit"
#define PNL_RESULT_FOOTER "any key: back"
#define PNL_SELECTED_STYLE PNL_BOLD
#define PNL_LABEL_MAX 24

struct pnl_result_view {
    int mark;
    long long shown_ms;
    char *text;
};

static const int pnl_mark_roles[] = {PNL_ROLE_OK, PNL_ROLE_WARN, PNL_ROLE_FAIL};

void pnl_menu_clear_lines(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->lines_len; i++) {
        pnl_mem_free(&app->mem, app->lines[i].label);
        pnl_mem_free(&app->mem, app->lines[i].text);
    }
    app->lines_len = 0;
}

int pnl_valid_mark(int mark)
{
    return mark == PNL_MARK_OK || mark == PNL_MARK_WAIT || mark == PNL_MARK_FAIL;
}

int pnl_label_width(const pnl_app_t *app)
{
    int width = 0;
    size_t i;

    for (i = 0; i < app->lines_len; i++) {
        int w = app->lines[i].label != NULL ? pnl_text_width(app->lines[i].label) : 0;

        if (w > width)
            width = w;
    }
    return width < PNL_LABEL_MAX ? width : PNL_LABEL_MAX;
}

int pnl_status_line(pnl_app_t *app, int mark, const char *label, const char *text)
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
    line->label = label != NULL && label[0] != '\0' ? pnl_mem_copy(&app->mem, label) : NULL;
    if (line->text == NULL || (line->label == NULL && label != NULL && label[0] != '\0')) {
        pnl_mem_free(&app->mem, line->text);
        pnl_mem_free(&app->mem, line->label);
        return PNL_ERR_NOMEM;
    }
    line->mark = mark;
    line->meter = -1.0;
    app->lines_len++;
    return PNL_OK;
}

void pnl_draw_mark(pnl_canvas_t *canvas, int y, int mark, const char *text)
{
    static const char *const marks[] = {"ok", "..", "!!"};
    if (mark == PNL_MARK_WAIT && canvas->screen->spin >= 0) {
        pnl_draw_spinner(canvas, 2, y, PNL_FG(pnl_mark_roles[mark]));
    } else if (mark == PNL_MARK_FAIL && canvas->screen->pulse >= 0) {
        canvas->screen->spinning = 1;
        pnl_canvas_text(canvas, 2, y, PNL_FG(PNL_FADE_FIRST + (PNL_ROLE_FAIL - PNL_ROLE_ACCENT) * PNL_FADE_STEPS +
                                             canvas->screen->pulse),
                        marks[mark]);
    } else
        pnl_canvas_text(canvas, 2, y, PNL_FG(pnl_mark_roles[mark]), marks[mark]);
    pnl_canvas_text(canvas, 6, y, 0, text);
}

#define PNL_METER_WIDTH 12

static void pnl_draw_line(pnl_canvas_t *canvas, int y, const struct pnl_line *line, int label_width,
                          int highlight)
{
    char percent[8];
    int x = line->label != NULL ? 6 + label_width + 2 : 6;

    pnl_draw_mark(canvas, y, line->mark, "");
    if (line->label != NULL)
        pnl_canvas_text(canvas, 6, y, 0, line->label);
    if (line->label != NULL && pnl_text_width(line->label) > label_width)
        pnl_canvas_text(canvas, 6 + label_width - 1, y, PNL_FG(PNL_ROLE_MUTED), "\xe2\x80\xa6");
    pnl_canvas_text(canvas, x, y, highlight, line->text);
    if (line->meter < 0.0)
        return;
    if (line->text[0] != '\0')
        x += pnl_text_width(line->text) + 2;
    pnl_canvas_bar(canvas, x, y, PNL_METER_WIDTH, line->meter, PNL_METER_LEVEL);
    snprintf(percent, sizeof(percent), "%3d%%", (int)(line->meter * 100 + 0.5));
    pnl_canvas_text(canvas, x + PNL_METER_WIDTH + 1, y, PNL_FG(PNL_ROLE_MUTED), percent);
}

static unsigned long pnl_hash(unsigned long hash, const char *text)
{
    const unsigned char *p;

    for (p = (const unsigned char *)text; *p != '\0'; p++)
        hash = hash * 33UL + *p;
    return hash;
}

static unsigned long pnl_line_id(const struct pnl_line *line, size_t index)
{
    return line->label != NULL ? pnl_hash(5381UL, line->label) : 5381UL * 31UL + (unsigned long)index;
}

static const struct pnl_seen *pnl_seen_find(const pnl_app_t *app, unsigned long id)
{
    size_t i;

    for (i = 0; i < app->seen_len; i++) {
        if (app->seen[i].id == id)
            return &app->seen[i];
    }
    return NULL;
}

static const char *pnl_number_at(const char *text, double *number)
{
    const char *at = text;
    char *end;

    while (*at != '\0' && !(*at >= '0' && *at <= '9'))
        at++;
    if (*at == '\0')
        return NULL;
    if (at > text && at[-1] == '-')
        at--;
    *number = strtod(at, &end);
    return end > at ? at : NULL;
}

static double pnl_count_shown(pnl_app_t *app, const struct pnl_seen *seen)
{
    double t = seen->count_ms != 0 ? pnl_ease_out(pnl_app_tween(app, seen->count_ms, PNL_COUNT_MS)) : 1.0;

    return seen->count_from + (seen->count_to - seen->count_from) * t;
}

static double pnl_meter_shown(pnl_app_t *app, const struct pnl_seen *seen)
{
    double t = seen->meter_ms != 0 ? pnl_ease_out(pnl_app_tween(app, seen->meter_ms, PNL_COUNT_MS)) : 1.0;

    return seen->meter_from + (seen->meter_to - seen->meter_from) * t;
}

static unsigned long pnl_prefix_hash(const char *text, const char *end)
{
    unsigned long hash = 5381UL + (unsigned long)(end - text);

    for (; text < end; text++)
        hash = hash * 33UL + (unsigned char)*text;
    return hash | 1UL;
}

static void pnl_track_count(pnl_app_t *app, struct pnl_seen *next, const struct pnl_seen *old, const char *text)
{
    double number = 0.0;
    const char *at = pnl_number_at(text, &number);

    next->count_to = number;
    next->count_from = number;
    next->count_ms = 0;
    next->count_prefix = at != NULL ? pnl_prefix_hash(text, at) : 0;
    if (at == NULL || old == NULL || old->count_prefix != next->count_prefix)
        return;
    next->count_from = pnl_count_shown(app, old);
    next->count_ms = pnl_app_frame_ms(app);
}

static void pnl_track_meter(pnl_app_t *app, struct pnl_seen *next, const struct pnl_seen *old, double meter)
{
    next->meter_to = meter;
    next->meter_from = meter;
    next->meter_ms = 0;
    if (old == NULL || old->meter_to == meter || meter < 0.0 || old->meter_to < 0.0)
        return;
    next->meter_from = pnl_meter_shown(app, old);
    next->meter_ms = pnl_app_frame_ms(app);
}

static void pnl_track_motion(pnl_app_t *app, struct pnl_seen *next, const struct pnl_seen *old,
                             const struct pnl_line *line)
{
    if (old != NULL && old->hash == next->hash) {
        next->count_ms = old->count_ms;
        next->count_prefix = old->count_prefix;
        next->count_from = old->count_from;
        next->count_to = old->count_to;
    } else {
        pnl_track_count(app, next, old, line->text);
    }
    pnl_track_meter(app, next, old, line->meter);
    if (old != NULL && old->meter_to == line->meter) {
        next->meter_ms = old->meter_ms;
        next->meter_from = old->meter_from;
    }
}

static void pnl_menu_track(pnl_app_t *app)
{
    long long now = pnl_app_frame_ms(app);
    struct pnl_seen *swap;
    size_t cap;
    size_t i;

    if (pnl_mem_grow(&app->mem, (void **)&app->seen_next, &app->seen_next_cap, app->lines_len,
                     sizeof(*app->seen_next)) != PNL_OK)
        return;
    for (i = 0; i < app->lines_len; i++) {
        struct pnl_seen *next = &app->seen_next[i];
        const struct pnl_seen *old;

        next->id = pnl_line_id(&app->lines[i], i);
        next->hash = pnl_hash(5381UL + (unsigned long)app->lines[i].mark, app->lines[i].text);
        old = pnl_seen_find(app, next->id);
        if (old == NULL)
            next->changed_ms = app->seen_ready ? now : 0;
        else
            next->changed_ms = old->hash != next->hash ? now : old->changed_ms;
        pnl_track_motion(app, next, old, &app->lines[i]);
    }
    swap = app->seen;
    cap = app->seen_cap;
    app->seen = app->seen_next;
    app->seen_cap = app->seen_next_cap;
    app->seen_next = swap;
    app->seen_next_cap = cap;
    app->seen_len = app->lines_len;
    app->seen_ready = 1;
}

void pnl_menu_refresh(pnl_app_t *app)
{
    pnl_menu_clear_lines(app);
    if (app->status_fn != NULL) {
        app->in_status = 1;
        app->status_fn(app->status_ctx, app);
        app->in_status = 0;
    }
    pnl_values_lines(app);
    if (app->running)
        pnl_menu_track(app);
}

static void pnl_draw_item(pnl_app_t *app, pnl_canvas_t *canvas, int y, size_t index)
{
    const char *label = index < app->actions_len ? app->actions[index].label : PNL_QUIT_LABEL;
    int style = index == app->selected ? PNL_SELECTED_STYLE : 0;
    char number[2] = {0, 0};

    if (app->chosen_ms != 0 && index == app->chosen)
        style |= pnl_fade_style(app, canvas, PNL_ROLE_ACCENT, app->chosen_ms, 2);

    if (index < 9 && index < app->actions_len)
        number[0] = (char)('1' + index);
    else if (index == app->actions_len)
        number[0] = 'q';
    pnl_canvas_text(canvas, 4, y, PNL_FG(PNL_ROLE_MUTED), number);
    pnl_canvas_text(canvas, 6, y, style, label);
}

static void pnl_cascade_item(pnl_app_t *app, pnl_canvas_t *canvas, int y, size_t index)
{
    struct pnl_canvas row = *canvas;
    size_t step = index - app->menu_top < PNL_CASCADE_STEPS ? index - app->menu_top : PNL_CASCADE_STEPS;
    double t = 1.0;

    if (pnl_motion_on(app, PNL_MOTION_ENTER))
        t = pnl_app_tween(app, app->cascade_ms + (long long)step * PNL_CASCADE_STEP_MS, PNL_ENTER_MS);
    if (t <= 0.0)
        return;
    row.x += (int)((1.0 - pnl_ease_out(t)) * 8 + 0.5);
    pnl_draw_item(app, &row, y, index);
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

static void pnl_count_text(pnl_app_t *app, const struct pnl_seen *seen, const char *text, char *out, size_t out_len)
{
    double number;
    const char *at = pnl_number_at(text, &number);
    char *end;
    const char *dot;
    int decimals = 0;

    strtod(at, &end);
    dot = (memchr)(at, '.', (size_t)(end - at));
    if (dot != NULL)
        decimals = (int)(end - dot - 1);
    snprintf(out, out_len, "%.*s%.*f%s", (int)(at - text), text, decimals, pnl_count_shown(app, seen), end);
}

static struct pnl_line pnl_shown_line(pnl_app_t *app, size_t i, char *text, size_t text_len)
{
    struct pnl_line line = app->lines[i];
    const struct pnl_seen *seen = i < app->seen_len ? &app->seen[i] : NULL;

    if (seen == NULL || !pnl_motion_on(app, PNL_MOTION_COUNT))
        return line;
    if (seen->count_ms != 0 && pnl_app_tween(app, seen->count_ms, PNL_COUNT_MS) < 1.0) {
        pnl_count_text(app, seen, line.text, text, text_len);
        line.text = text;
    }
    if (seen->meter_ms != 0)
        line.meter = pnl_meter_shown(app, seen);
    return line;
}

void pnl_menu_draw(pnl_app_t *app, pnl_canvas_t *canvas)
{
    size_t i;
    int y = 0;
    int first;
    int marker;
    int rows;
    int label_width;

    if (!app->anim_frame)
        pnl_menu_refresh(app);
    label_width = pnl_label_width(app);
    y = pnl_banner_draw(app, canvas, (int)(app->lines_len + app->actions_len) + 2);
    for (i = 0; i < app->lines_len; i++) {
        int highlight = i < app->seen_len && app->seen[i].changed_ms != 0
                            ? pnl_fade_style(app, canvas, PNL_ROLE_ACCENT, app->seen[i].changed_ms, 9)
                            : 0;

        char text[PNL_CELL_MAX];
        struct pnl_line line = pnl_shown_line(app, i, text, sizeof(text));

        pnl_draw_line(canvas, y++, &line, label_width, highlight);
    }
    if (app->lines_len > 0)
        y++;
    rows = pnl_canvas_height(canvas) - y;
    pnl_menu_scroll(app, rows > 0 ? (size_t)rows : 0);
    first = y;
    app->menu_y = canvas->y + first;
    for (i = app->menu_top; i <= app->actions_len && y < pnl_canvas_height(canvas); i++)
        pnl_cascade_item(app, canvas, y++, i);
    app->menu_end = canvas->y + y;
    pnl_glide_to(app, &app->select_glide, (double)app->selected, PNL_MOTION_SLIDE);
    marker = first + (int)(pnl_glide_at(app, &app->select_glide, PNL_MOTION_SLIDE) + 0.5) - (int)app->menu_top;
    if (marker >= first && marker < y)
        pnl_canvas_text(canvas, 2, marker, PNL_SELECTED_STYLE | PNL_FG(PNL_ROLE_ACCENT), ">");
    if (app->menu_top > 0)
        pnl_canvas_text(canvas, 0, y - rows, PNL_FG(PNL_ROLE_MUTED), "^");
    if (i <= app->actions_len)
        pnl_canvas_text(canvas, 0, y - 1, PNL_FG(PNL_ROLE_MUTED), "v");
}

static int pnl_menu_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int pnl_menu_jump(pnl_app_t *app, int key)
{
    size_t i;

    for (i = 1; i <= app->actions_len; i++) {
        size_t index = (app->selected + i) % (app->actions_len + 1);

        if (index < app->actions_len &&
            pnl_menu_lower((unsigned char)app->actions[index].label[0]) == pnl_menu_lower(key)) {
            app->selected = index;
            return 1;
        }
    }
    return 0;
}

static void pnl_menu_run(pnl_app_t *app, size_t index)
{
    app->selected = index;
    app->chosen = index;
    app->chosen_ms = pnl_motion_on(app, PNL_MOTION_FEEDBACK) ? pnl_term_now_ms() : 0;
    app->actions[index].fn(app->actions[index].ctx, app);
}

int pnl_menu_key(pnl_app_t *app, int key)
{
    size_t count = app->actions_len + 1;

    if (key >= '1' && key <= '9' && (size_t)(key - '1') < app->actions_len)
        pnl_menu_run(app, (size_t)(key - '1'));
    else if (key == PNL_KEY_HOME || key == PNL_KEY_PAGE_UP)
        app->selected = 0;
    else if (key == PNL_KEY_END || key == PNL_KEY_PAGE_DOWN)
        app->selected = app->actions_len;
    else if (key == PNL_KEY_UP || key == 'k')
        app->selected = (app->selected + count - 1) % count;
    else if (key == PNL_KEY_DOWN || key == 'j')
        app->selected = (app->selected + 1) % count;
    else if (key == 'q' || key == PNL_KEY_ESCAPE || (key == PNL_KEY_ENTER && app->selected == app->actions_len))
        app->quit = 1;
    else if (key == PNL_KEY_ENTER)
        pnl_menu_run(app, app->selected);
    else if (key < 0x80 && ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')))
        return pnl_menu_jump(app, key);
    else
        return 0;
    return 1;
}

int pnl_menu_mouse(pnl_app_t *app, int key)
{
    int y = app->term.mouse_y;

    if (key == PNL_KEY_WHEEL_UP && app->selected > 0)
        app->selected--;
    else if (key == PNL_KEY_WHEEL_DOWN && app->selected < app->actions_len)
        app->selected++;
    if (key != PNL_KEY_CLICK || y < app->menu_y || y >= app->menu_end)
        return 1;
    app->selected = app->menu_top + (size_t)(y - app->menu_y);
    return pnl_menu_key(app, PNL_KEY_ENTER);
}

static void pnl_result_type(pnl_app_t *app, pnl_canvas_t *canvas, const struct pnl_result_view *view)
{
    size_t len = strlen(view->text);
    size_t chars = 0;
    size_t shown;
    size_t cut;
    size_t i;
    double t;

    if (!pnl_motion_on(app, PNL_MOTION_TYPEWRITER))
        return;
    for (i = 0; i < len; i++)
        chars += ((unsigned char)view->text[i] & 0xc0) != 0x80;
    t = pnl_app_tween(app, view->shown_ms, chars * PNL_TYPE_CHAR_MS < PNL_TYPE_MAX_MS ? (int)chars * PNL_TYPE_CHAR_MS
                                                                                       : PNL_TYPE_MAX_MS);
    if (t >= 1.0)
        return;
    shown = (size_t)(t * (double)chars);
    for (cut = 0; cut < len; cut++) {
        if (((unsigned char)view->text[cut] & 0xc0) != 0x80 && shown-- == 0)
            break;
    }
    pnl_canvas_fill(canvas, 6 + pnl_text_width_n(view->text, cut), 0, pnl_canvas_width(canvas), 1, 0);
}

static void pnl_result_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_result_view *view = ctx;
    int flash;

    if (view->shown_ms == 0)
        view->shown_ms = pnl_app_frame_ms(app);
    pnl_draw_mark(canvas, 0, view->mark, view->text);
    pnl_result_type(app, canvas, view);
    flash = pnl_fade_style(app, canvas, pnl_mark_roles[view->mark], view->shown_ms, 4);
    if (flash != 0)
        pnl_canvas_text(canvas, 1, 0, flash | PNL_BOLD, view->mark == PNL_MARK_OK ? " ok " : view->mark == PNL_MARK_FAIL ? " !! " : " .. ");
}

static int pnl_result_key(void *ctx, pnl_app_t *app, int key)
{
    (void)ctx;
    if (key != PNL_KEY_WHEEL_UP && key != PNL_KEY_WHEEL_DOWN)
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
    result->shown_ms = 0;
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
