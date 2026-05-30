#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_BOX_TL "\xe2\x95\xad"
#define PNL_BOX_TR "\xe2\x95\xae"
#define PNL_BOX_BL "\xe2\x95\xb0"
#define PNL_BOX_BR "\xe2\x95\xaf"
#define PNL_BOX_H "\xe2\x94\x80"
#define PNL_BOX_V "\xe2\x94\x82"
#define PNL_TOAST_MS 3000
#define PNL_TOAST_SLIDE_MS 180

static const char *const pnl_toast_marks[] = {"ok", "..", "!!"};
static const int pnl_toast_roles[] = {PNL_ROLE_OK, PNL_ROLE_WARN, PNL_ROLE_FAIL};

static void pnl_box_edge(struct pnl_screen *screen, int x, int y, int width, int style, const char *left,
                         const char *right)
{
    int i;

    pnl_screen_text(screen, x, y, 1, (uint32_t)style, left);
    for (i = 1; i < width - 1; i++)
        pnl_screen_text(screen, x + i, y, 1, (uint32_t)style, PNL_BOX_H);
    pnl_screen_text(screen, x + width - 1, y, 1, (uint32_t)style, right);
}

static void pnl_box(struct pnl_screen *screen, int x, int y, int width, int height, int style, const char *title)
{
    int row;

    pnl_box_edge(screen, x, y, width, style, PNL_BOX_TL, PNL_BOX_TR);
    for (row = y + 1; row < y + height - 1; row++) {
        pnl_screen_fill(screen, x, row, width, 0);
        pnl_screen_text(screen, x, row, 1, (uint32_t)style, PNL_BOX_V);
        pnl_screen_text(screen, x + width - 1, row, 1, (uint32_t)style, PNL_BOX_V);
    }
    pnl_box_edge(screen, x, y + height - 1, width, style, PNL_BOX_BL, PNL_BOX_BR);
    if (title != NULL && width > 6) {
        int end = pnl_screen_text(screen, x + 2, y, width - 4, PNL_BOLD, " ");

        end = pnl_screen_text(screen, end, y, x + width - 3 - end, PNL_BOLD, title);
        pnl_screen_text(screen, end, y, 1, PNL_BOLD, " ");
    }
}

static int pnl_clamp_size(int want, int room)
{
    return want < 1 ? 1 : want > room ? room : want;
}

static void pnl_box_scaled(struct pnl_screen *screen, int x, int y, int width, int height, double scale, int style)
{
    int w = (int)(width * scale + 0.5);
    int h = (int)(height * scale + 0.5);

    w = w < 5 ? 5 : w;
    h = h < 3 ? 3 : h;
    pnl_box(screen, x + (width - w) / 2, y + (height - h) / 2, w, h, style, NULL);
}

void pnl_ghost_draw(pnl_app_t *app)
{
    struct pnl_ghost *ghost = &app->ghost;
    double t;

    if (ghost->ms == 0)
        return;
    t = pnl_app_tween(app, ghost->ms, PNL_EXIT_MS);
    if (t >= 1.0 || !pnl_motion_on(app, PNL_MOTION_ENTER)) {
        ghost->ms = 0;
        return;
    }
    pnl_box_scaled(&app->screen, ghost->x, ghost->y, ghost->width, ghost->height, 1.0 - 0.6 * pnl_ease_out(t),
                   PNL_FG(PNL_ROLE_MUTED));
}

void pnl_dialog_draw(pnl_app_t *app, const pnl_canvas_t *area, struct pnl_view *view, int focused)
{
    struct pnl_canvas inner;
    int width = pnl_clamp_size(view->dialog_width + 4, area->width - 2);
    int height = pnl_clamp_size(view->dialog_height + 2, area->height);
    int x = area->x + (area->width - width) / 2;
    int y = area->y + (area->height - height) / 2;
    int style = PNL_FG(focused ? PNL_ROLE_ACCENT : PNL_ROLE_MUTED);
    double t = pnl_motion_on(app, PNL_MOTION_ENTER) ? pnl_app_tween(app, view->shown_ms, PNL_ENTER_MS) : 1.0;

    if (width < 5 || height < 3)
        return;
    view->box_x = x;
    view->box_y = y;
    view->box_width = width;
    view->box_height = height;
    if (t < 1.0) {
        pnl_box_scaled(area->screen, x, y, width, height, 0.4 + 0.6 * pnl_ease_out(t), style);
        return;
    }
    pnl_box(area->screen, x, y, width, height, style, view->title);
    inner.screen = area->screen;
    inner.focused = focused;
    inner.x = x + 2;
    inner.y = y + 1;
    inner.width = width - 4;
    inner.height = height - 2;
    view->origin_x = inner.x;
    view->origin_y = inner.y;
    view->draw_fn(view->ctx, app, &inner);
}

int pnl_toast(pnl_app_t *app, int mark, const char *text)
{
    struct pnl_toast *toast;

    if (app == NULL || text == NULL || !pnl_valid_mark(mark))
        return PNL_ERR_ARG;
    if (app->cli) {
        if (!app->cli_json)
            fprintf(stderr, "[%s] %s\n", pnl_toast_marks[mark], text);
        return PNL_OK;
    }
    if (app->toasts_len == PNL_TOASTS) {
        memmove(app->toasts, app->toasts + 1, sizeof(app->toasts) - sizeof(app->toasts[0]));
        app->toasts_len--;
    }
    toast = &app->toasts[app->toasts_len++];
    toast->mark = mark;
    toast->shown_ms = pnl_term_now_ms();
    snprintf(toast->text, sizeof(toast->text), "%s", text);
    app->dirty = 1;
    return PNL_OK;
}

long long pnl_toasts_next_ms(const pnl_app_t *app)
{
    long long gone;

    if (app->toasts_len == 0)
        return -1;
    gone = app->toasts[0].shown_ms + PNL_TOAST_MS;
    if (pnl_motion_on(app, PNL_MOTION_ENTER) && gone - PNL_EXIT_MS > pnl_term_now_ms())
        return gone - PNL_EXIT_MS;
    return gone;
}

void pnl_toasts_expire(pnl_app_t *app)
{
    long long now = pnl_term_now_ms();
    size_t gone = 0;

    while (gone < app->toasts_len && app->toasts[gone].shown_ms + PNL_TOAST_MS <= now)
        gone++;
    if (gone < app->toasts_len && pnl_motion_on(app, PNL_MOTION_ENTER) &&
        app->toasts[gone].shown_ms + PNL_TOAST_MS - PNL_EXIT_MS <= now)
        pnl_app_animate(app, PNL_EXIT_MS);
    if (gone == 0)
        return;
    memmove(app->toasts, app->toasts + gone, (app->toasts_len - gone) * sizeof(app->toasts[0]));
    app->toasts_len -= gone;
    app->dirty = 1;
}

static void pnl_toast_draw(pnl_app_t *app, const struct pnl_toast *toast, int y)
{
    struct pnl_screen *screen = &app->screen;
    int width = pnl_text_width(toast->text) + 7;
    int style = PNL_BG(PNL_ROLE_SURFACE);
    double t = pnl_motion_on(app, PNL_MOTION_TRANSITION) ? pnl_app_tween(app, toast->shown_ms, PNL_TOAST_SLIDE_MS)
                                                         : 1.0;
    long long left = toast->shown_ms + PNL_TOAST_MS - pnl_app_frame_ms(app);
    int x;

    if (pnl_motion_on(app, PNL_MOTION_ENTER) && left < PNL_EXIT_MS)
        t = left <= 0 ? 0.0 : t * pnl_ease_out((double)left / PNL_EXIT_MS);
    if (t <= 0.0)
        return;
    if (width > screen->width - 2)
        width = screen->width - 2;
    x = screen->width - 1 - (int)(t * width + 0.5);
    pnl_screen_fill(screen, x, y, width, (uint32_t)style);
    pnl_screen_text(screen, x + 1, y, -1, (uint32_t)(style | PNL_BOLD | PNL_FG(pnl_toast_roles[toast->mark])),
                    pnl_toast_marks[toast->mark]);
    pnl_screen_text(screen, x + 5, y, screen->width - x - 6, (uint32_t)style, toast->text);
}

void pnl_toasts_draw(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->toasts_len; i++)
        pnl_toast_draw(app, &app->toasts[i], app->screen.height - 2 - (int)(app->toasts_len - 1 - i));
}
