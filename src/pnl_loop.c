#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_MENU_FOOTER "up/down: move  enter: choose  ?: help  q: quit"
#define PNL_VIEW_FOOTER "esc: back"
#define PNL_TITLE_STYLE PNL_BOLD
#define PNL_FOOTER_STYLE PNL_FG(PNL_ROLE_MUTED)

static void pnl_transition_start(pnl_app_t *app, int divisor)
{
    if (!app->running || !pnl_motion_on(app, PNL_MOTION_TRANSITION))
        return;
    app->transition_ms = pnl_term_now_ms();
    app->transition_shift = app->screen.width / divisor;
}

static int pnl_pulse_step(const pnl_app_t *app)
{
    int phase;
    int depth = app->screen.palette.depth;

    if (!pnl_motion_on(app, PNL_MOTION_PULSE) || (depth != PNL_DEPTH_TRUE && depth != PNL_DEPTH_256))
        return -1;
    phase = (int)(app->frame_ms % PNL_PULSE_MS) * 2 * PNL_PULSE_DEPTH / PNL_PULSE_MS;
    return phase < PNL_PULSE_DEPTH ? phase : 2 * PNL_PULSE_DEPTH - phase;
}

static int pnl_transition_offset(pnl_app_t *app)
{
    double t;

    if (app->transition_shift == 0)
        return 0;
    t = pnl_app_tween(app, app->transition_ms, PNL_TRANSITION_MS);
    if (t >= 1.0)
        app->transition_shift = 0;
    t = 1.0 - (1.0 - t) * (1.0 - t);
    return (int)((1.0 - t) * app->transition_shift + 0.5);
}

int pnl_view_push_internal(pnl_app_t *app, const struct pnl_view *view)
{
    int status;

    if (app->cli) {
        app->cli_needs_tty = 1;
        return PNL_ERR_TTY;
    }
    status = pnl_mem_grow(&app->mem, (void **)&app->views, &app->views_cap, app->views_len + 1,
                              sizeof(*app->views));

    if (status != PNL_OK)
        return status;
    app->views[app->views_len++] = *view;
    if (view->dialog_width == 0)
        pnl_transition_start(app, 3);
    app->dirty = 1;
    return PNL_OK;
}

int pnl_view_push(pnl_app_t *app, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *view_ctx)
{
    struct pnl_view view;
    int status;

    if (app == NULL || draw_fn == NULL)
        return PNL_ERR_ARG;
    memset(&view, 0, sizeof(view));
    view.draw_fn = draw_fn;
    view.key_fn = key_fn;
    view.ctx = view_ctx;
    if (title != NULL) {
        view.title = pnl_mem_copy(&app->mem, title);
        if (view.title == NULL)
            return PNL_ERR_NOMEM;
    }
    status = pnl_view_push_internal(app, &view);
    if (status != PNL_OK)
        pnl_mem_free(&app->mem, view.title);
    return status;
}

int pnl_view_push_dialog(pnl_app_t *app, const char *title, int width, int height, pnl_draw_fn draw_fn,
                         pnl_key_fn key_fn, void *view_ctx)
{
    struct pnl_view view;
    int status;

    if (app == NULL || draw_fn == NULL || width <= 0 || height <= 0)
        return PNL_ERR_ARG;
    memset(&view, 0, sizeof(view));
    view.draw_fn = draw_fn;
    view.key_fn = key_fn;
    view.ctx = view_ctx;
    view.dialog_width = width;
    view.dialog_height = height;
    if (title != NULL) {
        view.title = pnl_mem_copy(&app->mem, title);
        if (view.title == NULL)
            return PNL_ERR_NOMEM;
    }
    status = pnl_view_push_internal(app, &view);
    if (status != PNL_OK)
        pnl_mem_free(&app->mem, view.title);
    return status;
}

int pnl_view_pop(pnl_app_t *app)
{
    struct pnl_view view;

    if (app == NULL)
        return PNL_ERR_ARG;
    if (app->views_len == 0)
        return PNL_ERR_NOT_FOUND;
    view = app->views[--app->views_len];
    pnl_mem_free(&app->mem, view.title);
    pnl_mem_free(&app->mem, view.hints);
    if (view.drop_fn != NULL)
        view.drop_fn(app, view.ctx);
    if (view.dialog_width == 0)
        pnl_transition_start(app, 6);
    app->dirty = 1;
    return PNL_OK;
}

int pnl_view_modal(pnl_app_t *app, struct pnl_view *view)
{
    size_t depth = app->views_len;
    int status = pnl_view_push_internal(app, view);

    if (status != PNL_OK)
        return status;
    status = pnl_loop(app, depth + 1);
    while (app->views_len > depth)
        pnl_view_pop(app);
    return status;
}

int pnl_view_set_hints(pnl_app_t *app, const char *hints)
{
    struct pnl_view *top;
    char *copy;

    if (app == NULL || hints == NULL)
        return PNL_ERR_ARG;
    if (app->views_len == 0)
        return PNL_ERR_NOT_FOUND;
    copy = pnl_mem_copy(&app->mem, hints);
    if (copy == NULL)
        return PNL_ERR_NOMEM;
    top = &app->views[app->views_len - 1];
    pnl_mem_free(&app->mem, top->hints);
    top->hints = copy;
    app->dirty = 1;
    return PNL_OK;
}

const char *pnl_view_hints(const pnl_app_t *app)
{
    const struct pnl_view *top;

    if (app->views_len == 0)
        return NULL;
    top = &app->views[app->views_len - 1];
    if (top->footer_ref != NULL)
        return *top->footer_ref;
    if (top->hints != NULL)
        return top->hints;
    return top->footer != NULL ? top->footer : PNL_VIEW_FOOTER;
}

void pnl_views_drop(pnl_app_t *app)
{
    while (app->views_len > 0)
        pnl_view_pop(app);
}

int pnl_app_add_timer(pnl_app_t *app, int interval_ms, pnl_timer_fn timer_fn, void *timer_ctx, int *out_timer)
{
    struct pnl_timer *timer;
    int status;

    if (app == NULL || interval_ms <= 0)
        return PNL_ERR_ARG;
    status = pnl_mem_grow(&app->mem, (void **)&app->timers, &app->timers_cap, app->timers_len + 1,
                          sizeof(*timer));
    if (status != PNL_OK)
        return status;
    timer = &app->timers[app->timers_len++];
    timer->id = app->next_timer++;
    timer->interval_ms = interval_ms;
    timer->due_ms = pnl_term_now_ms() + interval_ms;
    timer->fn = timer_fn;
    timer->ctx = timer_ctx;
    if (out_timer != NULL)
        *out_timer = timer->id;
    return PNL_OK;
}

int pnl_app_remove_timer(pnl_app_t *app, int timer)
{
    size_t i;

    if (app == NULL)
        return PNL_ERR_ARG;
    for (i = 0; i < app->timers_len; i++) {
        if (app->timers[i].id != timer)
            continue;
        memmove(&app->timers[i], &app->timers[i + 1], (app->timers_len - i - 1) * sizeof(*app->timers));
        app->timers_len--;
        return PNL_OK;
    }
    return PNL_ERR_NOT_FOUND;
}

int pnl_app_motion(const pnl_app_t *app)
{
    return app != NULL && app->motion;
}

long long pnl_app_frame_ms(const pnl_app_t *app)
{
    if (app == NULL)
        return 0;
    return app->frame_ms != 0 ? app->frame_ms : pnl_term_now_ms();
}

int pnl_app_enable_motion(pnl_app_t *app, int flags)
{
    if (app == NULL || (flags & ~PNL_MOTION_ALL) != 0)
        return PNL_ERR_ARG;
    app->motion_flags |= flags;
    return PNL_OK;
}

int pnl_motion_on(const pnl_app_t *app, int flag)
{
    return app->motion && (app->motion_flags & flag) != 0;
}

double pnl_glide_at(pnl_app_t *app, const struct pnl_glide *glide, int flag)
{
    double t;

    if (!pnl_motion_on(app, flag))
        return glide->to;
    t = pnl_app_tween(app, glide->start_ms, PNL_GLIDE_MS);
    t = 1.0 - (1.0 - t) * (1.0 - t);
    return glide->from + (glide->to - glide->from) * t;
}

void pnl_glide_to(pnl_app_t *app, struct pnl_glide *glide, double target, int flag)
{
    if (glide->to == target)
        return;
    glide->from = pnl_motion_on(app, flag) ? pnl_glide_at(app, glide, flag) : target;
    glide->to = target;
    glide->start_ms = pnl_term_now_ms();
}

int pnl_app_animate(pnl_app_t *app, int duration_ms)
{
    long long until;

    if (app == NULL || duration_ms < 0)
        return PNL_ERR_ARG;
    if (!app->motion)
        return PNL_OK;
    until = pnl_term_now_ms() + duration_ms;
    if (until > app->animate_until_ms)
        app->animate_until_ms = until;
    return PNL_OK;
}

double pnl_app_tween(pnl_app_t *app, long long start_ms, int duration_ms)
{
    long long elapsed;

    if (app == NULL || !app->motion || duration_ms <= 0)
        return 1.0;
    elapsed = pnl_app_frame_ms(app) - start_ms;
    if (elapsed >= duration_ms)
        return 1.0;
    pnl_app_animate(app, (int)(duration_ms - elapsed));
    return elapsed <= 0 ? 0.0 : (double)elapsed / duration_ms;
}

static long long pnl_frame_wait(const pnl_app_t *app, long long now)
{
    long long next_frame;

    if (app->animate_until_ms <= now)
        return app->tasks_running > 0 ? PNL_TASK_TICK_MS : -1;
    next_frame = app->frame_ms + PNL_FRAME_MS - now;
    return next_frame < 0 ? 0 : next_frame;
}

int pnl_timers_timeout(const pnl_app_t *app)
{
    long long now = pnl_term_now_ms();
    long long next = pnl_frame_wait(app, now);
    size_t i;

    if (app->toasts_len > 0 && (next < 0 || pnl_toasts_next_ms(app) - now < next))
        next = pnl_toasts_next_ms(app) > now ? pnl_toasts_next_ms(app) - now : 0;
    for (i = 0; i < app->timers_len; i++) {
        long long wait = app->timers[i].due_ms - now;

        if (next < 0 || wait < next)
            next = wait < 0 ? 0 : wait;
    }
    return next > 0x7fffffff ? 0x7fffffff : (int)next;
}

void pnl_timers_run(pnl_app_t *app)
{
    long long now = pnl_term_now_ms();
    size_t i;

    for (i = 0; i < app->timers_len; i++) {
        struct pnl_timer *timer = &app->timers[i];
        pnl_timer_fn fn = timer->fn;
        void *ctx = timer->ctx;

        if (timer->due_ms > now)
            continue;
        timer->due_ms += timer->interval_ms;
        if (timer->due_ms <= now)
            timer->due_ms = now + timer->interval_ms;
        app->dirty = 1;
        if (fn != NULL)
            fn(ctx, app);
    }
}

#if defined(_WIN32)
int pnl_app_watch_fd(pnl_app_t *app, int fd, pnl_fd_fn fd_fn, void *fd_ctx)
{
    (void)fd_ctx;
    if (app == NULL || fd < 0 || fd_fn == NULL)
        return PNL_ERR_ARG;
    return PNL_ERR_UNSUPPORTED;
}
#else
static size_t pnl_watch_index(const pnl_app_t *app, int fd)
{
    size_t i;

    for (i = 0; i < app->watches_len; i++) {
        if (app->watches[i].fd == fd)
            break;
    }
    return i;
}

int pnl_app_watch_fd(pnl_app_t *app, int fd, pnl_fd_fn fd_fn, void *fd_ctx)
{
    struct pnl_watch *watch;
    size_t i;
    int status;

    if (app == NULL || fd < 0 || fd_fn == NULL)
        return PNL_ERR_ARG;
    i = pnl_watch_index(app, fd);
    if (i == app->watches_len && app->watches_len == PNL_WATCH_MAX)
        return PNL_ERR_NOMEM;
    status = pnl_mem_grow(&app->mem, (void **)&app->watches, &app->watches_cap, i + 1, sizeof(*watch));
    if (status != PNL_OK)
        return status;
    watch = &app->watches[i];
    watch->fd = fd;
    watch->fn = fd_fn;
    watch->ctx = fd_ctx;
    if (i == app->watches_len)
        app->watches_len++;
    return PNL_OK;
}
#endif

int pnl_app_unwatch_fd(pnl_app_t *app, int fd)
{
    size_t i;

    if (app == NULL)
        return PNL_ERR_ARG;
    for (i = 0; i < app->watches_len; i++) {
        if (app->watches[i].fd != fd)
            continue;
        memmove(&app->watches[i], &app->watches[i + 1], (app->watches_len - i - 1) * sizeof(*app->watches));
        app->watches_len--;
        return PNL_OK;
    }
    return PNL_ERR_NOT_FOUND;
}

int pnl_app_redraw(pnl_app_t *app)
{
    if (app == NULL)
        return PNL_ERR_ARG;
    app->dirty = 1;
    return PNL_OK;
}

int pnl_app_quit(pnl_app_t *app)
{
    if (app == NULL)
        return PNL_ERR_ARG;
    app->quit = 1;
    return PNL_OK;
}

int pnl_app_with_terminal(pnl_app_t *app, pnl_terminal_fn terminal_fn, void *terminal_ctx)
{
    int status;

    if (app == NULL || terminal_fn == NULL)
        return PNL_ERR_ARG;
    if (!app->term.active) {
        terminal_fn(terminal_ctx);
        return PNL_OK;
    }
    pnl_term_leave(&app->term);
    terminal_fn(terminal_ctx);
    status = pnl_term_enter(&app->term);
    app->screen.front_valid = 0;
    return status;
}

static void pnl_draw_title(pnl_app_t *app, const struct pnl_view *top)
{
    int x = pnl_screen_text(&app->screen, 0, 0, -1, PNL_TITLE_STYLE, app->title);

    if (top == NULL || top->title == NULL)
        return;
    x = pnl_screen_text(&app->screen, x, 0, -1, PNL_FOOTER_STYLE, " / ");
    pnl_screen_text(&app->screen, x, 0, -1, PNL_TITLE_STYLE, top->title);
}

static void pnl_draw_footer(pnl_app_t *app, const char *footer, int in_dialog)
{
    const char *more = in_dialog ? "" : "?: help";
    int room = app->screen.width - pnl_text_width(more) - 2;
    const char *at = footer;
    const char *cut;
    size_t keep = 0;
    char text[512];

    if (pnl_text_width(footer) <= app->screen.width) {
        pnl_screen_text(&app->screen, 0, app->screen.height - 1, -1, PNL_FOOTER_STYLE, footer);
        return;
    }
    while ((cut = (strstr)(at, "  ")) != NULL && pnl_text_width_n(footer, (size_t)(cut - footer)) <= room) {
        keep = (size_t)(cut - footer);
        at = cut + 2;
    }
    snprintf(text, sizeof(text), "%.*s%s%s", (int)keep, footer, keep > 0 && more[0] ? "  " : "", more);
    pnl_screen_text(&app->screen, 0, app->screen.height - 1, -1, PNL_FOOTER_STYLE, text);
}

static size_t pnl_base_view(const pnl_app_t *app)
{
    size_t base = app->views_len;

    while (base > 0 && app->views[base - 1].dialog_width > 0)
        base--;
    return base;
}

static void pnl_draw_views(pnl_app_t *app, pnl_canvas_t *canvas)
{
    size_t base = pnl_base_view(app);
    size_t i;

    if (base > 0) {
        app->views[base - 1].origin_x = canvas->x;
        app->views[base - 1].origin_y = canvas->y;
    }
    if (base > 0)
        app->views[base - 1].draw_fn(app->views[base - 1].ctx, app, canvas);
    else
        pnl_menu_draw(app, canvas);
    for (i = base; i < app->views_len; i++)
        pnl_dialog_draw(app, canvas, &app->views[i], i + 1 == app->views_len);
}

static int pnl_draw(pnl_app_t *app)
{
    size_t base = pnl_base_view(app);
    const struct pnl_view *top = app->views_len ? &app->views[app->views_len - 1] : NULL;
    const char *footer = PNL_MENU_FOOTER;
    struct pnl_canvas canvas;
    int width;
    int height;
    int status;

    pnl_term_size(&width, &height);
    status = pnl_screen_resize(&app->screen, &app->mem, width, height);
    if (status != PNL_OK)
        return status;
    pnl_screen_clear(&app->screen);
    pnl_draw_title(app, base > 0 ? &app->views[base - 1] : NULL);
    app->frame_ms = pnl_term_now_ms();
    app->screen.spin = app->motion ? (int)(app->frame_ms / PNL_SPIN_MS % 1000) : -1;
    app->screen.spinning = 0;
    app->screen.pulse = pnl_pulse_step(app);
    canvas.screen = &app->screen;
    canvas.focused = base == app->views_len;
    canvas.x = pnl_transition_offset(app);
    canvas.y = 2;
    canvas.width = width;
    canvas.height = height > 4 ? height - 4 : 0;
    pnl_draw_views(app, &canvas);
    if (top != NULL)
        footer = pnl_view_hints(app);
    pnl_draw_footer(app, footer, top != NULL && top->dialog_width > 0);
    pnl_toasts_draw(app);
    app->dirty = 0;
    app->anim_frame = 0;
    if (app->screen.spinning)
        pnl_app_animate(app, PNL_SPIN_MS);
    return pnl_screen_render(&app->screen, &app->mem);
}

int pnl_is_mouse_key(int key)
{
    return key == PNL_KEY_CLICK || key == PNL_KEY_WHEEL_UP || key == PNL_KEY_WHEEL_DOWN;
}

int pnl_mouse_position(const pnl_app_t *app, int *out_x, int *out_y)
{
    if (app == NULL || out_x == NULL || out_y == NULL)
        return PNL_ERR_ARG;
    *out_x = app->term.mouse_x - app->mouse_ox;
    *out_y = app->term.mouse_y - app->mouse_oy;
    return PNL_OK;
}

void pnl_wheel_keys(pnl_app_t *app, pnl_key_fn key_fn, void *ctx, int key)
{
    size_t depth = app->views_len;
    int i;

    for (i = 0; i < 3 && key != PNL_KEY_CLICK && app->views_len == depth; i++)
        key_fn(ctx, app, key == PNL_KEY_WHEEL_UP ? PNL_KEY_UP : PNL_KEY_DOWN);
}

static int pnl_outside_dialog(const pnl_app_t *app, const struct pnl_view *view)
{
    int x = app->term.mouse_x;
    int y = app->term.mouse_y;

    return view->dialog_width > 0 && (x < view->box_x || y < view->box_y || x >= view->box_x + view->box_width ||
                                      y >= view->box_y + view->box_height);
}

static void pnl_dispatch_mouse(pnl_app_t *app, int key)
{
    struct pnl_view top;

    if (app->views_len == 0) {
        pnl_menu_mouse(app, key);
        return;
    }
    top = app->views[app->views_len - 1];
    if (top.key_fn == NULL || pnl_outside_dialog(app, &top))
        return;
    app->mouse_ox = top.origin_x;
    app->mouse_oy = top.origin_y;
    if (!top.key_fn(top.ctx, app, key))
        pnl_wheel_keys(app, top.key_fn, top.ctx, key);
}

static void pnl_dispatch_key(pnl_app_t *app, int key)
{
    struct pnl_view top;

    app->dirty = 1;
    if (key == PNL_KEY_CTRL_C) {
        app->quit = 1;
        return;
    }
    if (pnl_is_mouse_key(key)) {
        pnl_dispatch_mouse(app, key);
        return;
    }
    if (app->views_len == 0 && pnl_menu_key(app, key))
        return;
    if (app->views_len > 0) {
        top = app->views[app->views_len - 1];
        if (top.key_fn != NULL && top.key_fn(top.ctx, app, key))
            return;
    }
    if (key == '?')
        pnl_help(app);
    else if (key == ':')
        pnl_palette(app);
    else if (app->views_len > 0 && (key == 'q' || key == PNL_KEY_ESCAPE))
        pnl_view_pop(app);
}

static int pnl_read_keys(pnl_app_t *app)
{
    int status = PNL_OK;

    while (status == PNL_OK && !app->quit) {
        int key = PNL_KEY_NONE;

        status = pnl_term_read_key(&app->term, 0, &key);
        if (status != PNL_OK || key == PNL_KEY_NONE)
            break;
        if (key == PNL_KEY_SUSPEND) {
            pnl_term_suspend(&app->term);
            app->screen.front_valid = 0;
            continue;
        }
        pnl_dispatch_key(app, key);
    }
    return status;
}

static void pnl_dispatch_watches(pnl_app_t *app, const unsigned char *ready)
{
    struct pnl_watch watches[PNL_WATCH_MAX];
    size_t count = app->watches_len;
    size_t i;

    if (count == 0)
        return;
    memcpy(watches, app->watches, count * sizeof(*watches));
    for (i = 0; i < count && !app->quit; i++) {
        if (!ready[i])
            continue;
        app->dirty = 1;
        watches[i].fn(watches[i].ctx, app, watches[i].fd);
    }
}

int pnl_loop(pnl_app_t *app, size_t min_views)
{
    unsigned char ready[PNL_WATCH_MAX];
    int status = PNL_OK;

    while (status == PNL_OK && !app->quit && app->views_len >= min_views) {
        int input;

        if (pnl_term_resized())
            app->screen.front_valid = 0;
        if (app->dirty || !app->screen.front_valid)
            status = pnl_draw(app);
        if (status != PNL_OK)
            break;
        memset(ready, 0, sizeof(ready));
        input = pnl_term_wait(&app->term, app->watches, app->watches_len, pnl_timers_timeout(app), ready);
        if (input < 0)
            return PNL_ERR_IO;
        if (app->term.woken) {
            app->term.woken = 0;
            app->dirty = 1;
        }
        if (input > 0)
            status = pnl_read_keys(app);
        pnl_dispatch_watches(app, ready);
        pnl_timers_run(app);
        pnl_toasts_expire(app);
        if (app->tasks_running > 0)
            app->dirty = 1;
        if (!app->dirty && (app->animate_until_ms > pnl_term_now_ms() || app->animate_until_ms > app->frame_ms)) {
            app->dirty = 1;
            app->anim_frame = 1;
        }
    }
    return status;
}

static void pnl_timers_restart(pnl_app_t *app)
{
    long long now = pnl_term_now_ms();
    size_t i;

    for (i = 0; i < app->timers_len; i++)
        app->timers[i].due_ms = now + app->timers[i].interval_ms;
}

static int pnl_link_slow(pnl_app_t *app)
{
    const char *ssh = getenv("SSH_CONNECTION");

    if (ssh == NULL || ssh[0] == '\0')
        return 0;
    if (app->reply_ms < 0)
        app->reply_ms = pnl_link_probe(&app->term);
    return app->reply_ms < 0 || app->reply_ms > PNL_SLOW_LINK_MS;
}

static int pnl_motion_detect(pnl_app_t *app)
{
    const char *motion = getenv("TUIKIT_MOTION");

    if (motion != NULL && strcmp(motion, "all") == 0)
        app->motion_flags = PNL_MOTION_ALL;
    if (motion != NULL && (strcmp(motion, "off") == 0 || strcmp(motion, "0") == 0))
        return 0;
    if (motion != NULL && (strcmp(motion, "on") == 0 || strcmp(motion, "all") == 0))
        return 1;
    return !pnl_link_slow(app);
}

static void pnl_run_palette(pnl_app_t *app)
{
    struct pnl_theme theme = app->theme;
    const char *chosen = getenv("TUIKIT_THEME");
    const char *contrast = getenv("TUIKIT_CONTRAST");
    int depth = pnl_color_detect();
    long background = -1;
    int known = pnl_background_detect(&app->term, depth, &background, &app->reply_ms);

    if (chosen != NULL && chosen[0] != '\0')
        pnl_theme_choose(&theme, chosen);
    pnl_palette_init(&app->screen.palette, &theme, depth, known && pnl_is_light(background), known ? background : -1,
                     contrast != NULL && strcmp(contrast, "high") == 0);
}

int pnl_run_interactive(pnl_app_t *app)
{
    const char *mouse = getenv("TUIKIT_MOUSE");
    int status;

    app->term.mouse = mouse == NULL || (strcmp(mouse, "off") != 0 && strcmp(mouse, "0") != 0);
    status = pnl_term_enter(&app->term);

    if (status != PNL_OK)
        return status;
    app->running = 1;
    app->quit = 0;
    app->selected = 0;
    app->menu_top = 0;
    pnl_run_palette(app);
    app->motion = pnl_motion_detect(app);
    app->select_glide.from = app->select_glide.to = 0.0;
    app->transition_shift = 0;
    app->seen_len = 0;
    app->seen_ready = 0;
    app->animate_until_ms = 0;
    app->dirty = 1;
    app->screen.front_valid = 0;
    pnl_timers_restart(app);
    status = pnl_loop(app, 0);
    app->running = 0;
    app->motion = 0;
    app->frame_ms = 0;
    pnl_term_leave(&app->term);
    pnl_views_drop(app);
    pnl_menu_clear_lines(app);
    return status;
}
