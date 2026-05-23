#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_INSPECT_FRAMES 60
#define PNL_INSPECT_EVENTS 16
#define PNL_INSPECT_REFRESH_MS 250
#define PNL_INSPECT_FOOTER "q: back  f12: close"

struct pnl_event {
    long long ms;
    int kind;
    int arg;
};

struct pnl_inspect {
    double frame_ms[PNL_INSPECT_FRAMES];
    size_t frame_bytes[PNL_INSPECT_FRAMES];
    unsigned long frames;
    struct pnl_event events[PNL_INSPECT_EVENTS];
    unsigned long events_len;
    int timer;
    int open;
};

static const char *const pnl_event_names[] = {"key", "wake", "timer", "watch", "resize"};

void pnl_inspect_start(pnl_app_t *app)
{
    if (app->inspect != NULL || pnl_env_off("TUIKIT_INSPECT"))
        return;
    app->inspect = pnl_mem_alloc(&app->mem, sizeof(*app->inspect));
    if (app->inspect != NULL)
        memset(app->inspect, 0, sizeof(*app->inspect));
}

void pnl_inspect_free(pnl_app_t *app)
{
    pnl_mem_free(&app->mem, app->inspect);
    app->inspect = NULL;
}

void pnl_inspect_frame(pnl_app_t *app, long long took_us, size_t bytes)
{
    struct pnl_inspect *inspect = app->inspect;

    if (inspect == NULL)
        return;
    inspect->frame_ms[inspect->frames % PNL_INSPECT_FRAMES] = (double)took_us / 1000.0;
    inspect->frame_bytes[inspect->frames % PNL_INSPECT_FRAMES] = bytes;
    inspect->frames++;
}

void pnl_inspect_event(pnl_app_t *app, int kind, int arg)
{
    struct pnl_inspect *inspect = app->inspect;
    struct pnl_event *event;

    if (inspect == NULL || (kind == PNL_EVENT_TIMER && arg == inspect->timer && inspect->open))
        return;
    event = &inspect->events[inspect->events_len++ % PNL_INSPECT_EVENTS];
    event->ms = pnl_term_now_ms();
    event->kind = kind;
    event->arg = arg;
}

static void pnl_inspect_line(pnl_canvas_t *canvas, int y, const char *label, const char *text)
{
    pnl_canvas_text(canvas, 1, y, PNL_FG(PNL_ROLE_MUTED), label);
    pnl_canvas_text(canvas, 13, y, 0, text);
}

static void pnl_inspect_frames(const struct pnl_inspect *inspect, pnl_canvas_t *canvas, int y)
{
    size_t count = inspect->frames < PNL_INSPECT_FRAMES ? (size_t)inspect->frames : PNL_INSPECT_FRAMES;
    double series[PNL_INSPECT_FRAMES];
    double total = 0.0;
    double worst = 0.0;
    double last = 0.0;
    size_t bytes = 0;
    char text[160];
    size_t i;

    for (i = 0; i < count; i++) {
        size_t slot = (size_t)((inspect->frames - count + i) % PNL_INSPECT_FRAMES);

        series[i] = inspect->frame_ms[slot];
        total += series[i];
        worst = series[i] > worst ? series[i] : worst;
        last = series[i];
        bytes = inspect->frame_bytes[slot];
    }
    snprintf(text, sizeof(text), "%lu drawn  last %.2f ms  average %.2f ms  worst %.2f ms  %zu bytes",
             inspect->frames, last, count > 0 ? total / (double)count : 0.0, worst, bytes);
    pnl_inspect_line(canvas, y, "frames", text);
    pnl_canvas_sparkline(canvas, 13, y + 1, pnl_canvas_width(canvas) - 14, series, count, worst > 0.0 ? worst : 1.0,
                         0);
}

static void pnl_inspect_views(const pnl_app_t *app, pnl_canvas_t *canvas, int y)
{
    char text[256];
    size_t len = (size_t)snprintf(text, sizeof(text), "menu");
    size_t i;

    for (i = 0; i + 1 < app->views_len && len < sizeof(text); i++) {
        const struct pnl_view *view = &app->views[i];

        len += (size_t)snprintf(text + len, sizeof(text) - len, " > %s%s", view->title ? view->title : "view",
                                view->dialog_width > 0 ? " (dialog)" : "");
    }
    pnl_inspect_line(canvas, y, "screens", text);
}

static void pnl_inspect_parts(const pnl_app_t *app, pnl_canvas_t *canvas, int y)
{
    char text[256];

    snprintf(text, sizeof(text),
             "%zu actions  %zu status lines  %zu values  %zu timers  %zu watches  %d tasks  %zu toasts  %zu keys",
             app->actions_len, app->lines_len, app->values_len, app->timers_len - 1, app->watches_len,
             app->tasks_running, app->toasts_len, app->bindings_len);
    pnl_inspect_line(canvas, y, "parts", text);
}

static void pnl_inspect_motion(const pnl_app_t *app, pnl_canvas_t *canvas, int y)
{
    long long left = app->animate_until_ms - pnl_term_now_ms();
    char text[160];

    snprintf(text, sizeof(text), "%s%s  spinner %s  %s", app->motion ? "on" : "off",
             app->motion_flags == PNL_MOTION_ALL ? " (all)" : "", app->screen.spinning ? "turning" : "still",
             left > 0 ? "animating" : "idle");
    pnl_inspect_line(canvas, y, "motion", text);
}

static void pnl_inspect_terminal(const pnl_app_t *app, pnl_canvas_t *canvas, int y)
{
    static const char *const depths[] = {"no color", "16 colors", "256 colors", "true color"};
    char reply[32] = "no reply";
    char text[200];

    if (app->reply_ms >= 0)
        snprintf(reply, sizeof(reply), "reply in %ld ms", app->reply_ms);
    snprintf(text, sizeof(text), "%dx%d  %s  %s background  mouse %s  %s  %d links  %s",
             app->screen.width, app->screen.height, depths[app->screen.palette.depth & 3],
             app->screen.palette.light ? "light" : "dark", app->term.mouse ? "on" : "off",
             !app->term.focus_known ? "focus unknown" : app->term.focused ? "focused" : "unfocused",
             app->screen.links_len, reply);
    pnl_inspect_line(canvas, y, "terminal", text);
}

static void pnl_inspect_event_text(const struct pnl_event *event, char *out, size_t out_len)
{
    char key[32];

    if (event->kind != PNL_EVENT_KEY) {
        snprintf(out, out_len, "%s %d", pnl_event_names[event->kind], event->arg);
        return;
    }
    if (event->arg == PNL_KEY_CLICK || event->arg == PNL_KEY_WHEEL_UP || event->arg == PNL_KEY_WHEEL_DOWN)
        snprintf(key, sizeof(key), "mouse");
    else
        pnl_key_label(event->arg, key, sizeof(key));
    snprintf(out, out_len, "key %s", key);
}

static void pnl_inspect_events(const struct pnl_inspect *inspect, pnl_canvas_t *canvas, int y)
{
    long long now = pnl_term_now_ms();
    int rows = pnl_canvas_height(canvas) - y - 1;
    unsigned long i;

    pnl_canvas_text(canvas, 1, y, PNL_FG(PNL_ROLE_MUTED), "events");
    for (i = 0; i < inspect->events_len && i < PNL_INSPECT_EVENTS && (int)i < rows; i++) {
        const struct pnl_event *event = &inspect->events[(inspect->events_len - 1 - i) % PNL_INSPECT_EVENTS];
        char when[32];
        char text[64];

        snprintf(when, sizeof(when), "%7.2f s ago", (double)(now - event->ms) / 1000.0);
        pnl_inspect_event_text(event, text, sizeof(text));
        pnl_canvas_text(canvas, 13, y + (int)i, PNL_FG(PNL_ROLE_MUTED), when);
        pnl_canvas_text(canvas, 27, y + (int)i, 0, text);
    }
}

static void pnl_inspect_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    const struct pnl_inspect *inspect = ctx;

    pnl_inspect_frames(inspect, canvas, 0);
    pnl_inspect_views(app, canvas, 3);
    pnl_inspect_parts(app, canvas, 4);
    pnl_inspect_motion(app, canvas, 5);
    pnl_inspect_terminal(app, canvas, 6);
    pnl_inspect_events(inspect, canvas, 8);
}

static void pnl_inspect_tick(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void pnl_inspect_drop(pnl_app_t *app, void *ctx)
{
    struct pnl_inspect *inspect = ctx;

    pnl_app_remove_timer(app, inspect->timer);
    inspect->timer = 0;
    inspect->open = 0;
}

void pnl_inspect_toggle(pnl_app_t *app)
{
    struct pnl_inspect *inspect = app->inspect;
    struct pnl_view view;

    if (inspect == NULL)
        return;
    if (inspect->open) {
        if (app->views[app->views_len - 1].ctx == inspect)
            pnl_view_pop(app);
        return;
    }
    if (pnl_app_add_timer(app, PNL_INSPECT_REFRESH_MS, pnl_inspect_tick, NULL, &inspect->timer) != PNL_OK)
        return;
    memset(&view, 0, sizeof(view));
    view.title = pnl_mem_copy(&app->mem, "inspector");
    view.footer = PNL_INSPECT_FOOTER;
    view.draw_fn = pnl_inspect_draw;
    view.drop_fn = pnl_inspect_drop;
    view.ctx = inspect;
    inspect->open = 1;
    if (view.title == NULL || pnl_view_push_internal(app, &view) != PNL_OK) {
        pnl_mem_free(&app->mem, view.title);
        pnl_inspect_drop(app, inspect);
    }
}
