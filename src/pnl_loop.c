#include "pnl_internal.h"

#include <string.h>

#define PNL_MENU_FOOTER "up/down: move  enter: choose  ?: help  q: quit"
#define PNL_VIEW_FOOTER "esc: back"
#define PNL_TITLE_STYLE PNL_BOLD
#define PNL_FOOTER_STYLE PNL_FG(PNL_ROLE_MUTED)

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

int pnl_timers_timeout(const pnl_app_t *app)
{
    long long now = pnl_term_now_ms();
    long long next = app->tasks_running > 0 ? PNL_TASK_TICK_MS : -1;
    size_t i;

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

static int pnl_draw(pnl_app_t *app)
{
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
    pnl_draw_title(app, top);
    canvas.screen = &app->screen;
    canvas.x = 0;
    canvas.y = 2;
    canvas.width = width;
    canvas.height = height > 4 ? height - 4 : 0;
    if (top != NULL)
        top->draw_fn(top->ctx, app, &canvas);
    else
        pnl_menu_draw(app, &canvas);
    if (top != NULL)
        footer = pnl_view_hints(app);
    pnl_screen_text(&app->screen, 0, height - 1, -1, PNL_FOOTER_STYLE, footer);
    app->dirty = 0;
    return pnl_screen_render(&app->screen, &app->mem);
}

static void pnl_dispatch_key(pnl_app_t *app, int key)
{
    struct pnl_view top;

    app->dirty = 1;
    if (key == PNL_KEY_CTRL_C) {
        app->quit = 1;
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
        if (input > 0)
            status = pnl_read_keys(app);
        pnl_dispatch_watches(app, ready);
        pnl_timers_run(app);
        if (app->tasks_running > 0)
            app->dirty = 1;
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

static void pnl_run_palette(pnl_app_t *app)
{
    int depth = pnl_color_detect();
    long background = -1;
    int known = pnl_background_detect(&app->term, depth, &background);

    pnl_palette_init(&app->screen.palette, depth, known && pnl_is_light(background), known ? background : -1);
}

int pnl_run_interactive(pnl_app_t *app)
{
    int status = pnl_term_enter(&app->term);

    if (status != PNL_OK)
        return status;
    app->running = 1;
    app->quit = 0;
    app->selected = 0;
    app->menu_top = 0;
    pnl_run_palette(app);
    app->dirty = 1;
    app->screen.front_valid = 0;
    pnl_timers_restart(app);
    status = pnl_loop(app, 0);
    app->running = 0;
    pnl_term_leave(&app->term);
    pnl_views_drop(app);
    pnl_menu_clear_lines(app);
    return status;
}
