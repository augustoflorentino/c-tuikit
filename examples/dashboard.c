#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <string.h>

#define SERVICES 4
#define HISTORY 120
#define LOG_LINES 64

static const char *const names[SERVICES] = {"api", "worker", "database", "cache"};

struct dashboard {
    int tick;
    size_t selected;
    double load[SERVICES][HISTORY];
    size_t load_len;
    char log[LOG_LINES][80];
    size_t log_len;
};

static double sample(int tick, size_t service)
{
    int x = (tick * (int)(service * 7 + 13) + (int)service * 31) % 100;

    return (double)(x < 0 ? -x : x) * (service == 2 ? 0.95 : 0.6);
}

static void add_log(struct dashboard *dash, const char *line)
{
    if (dash->log_len == LOG_LINES) {
        memmove(dash->log, dash->log + 1, sizeof(dash->log) - sizeof(dash->log[0]));
        dash->log_len--;
    }
    snprintf(dash->log[dash->log_len++], sizeof(dash->log[0]), "%s", line);
}

static double current(const struct dashboard *dash, size_t service)
{
    return dash->load_len ? dash->load[service][(dash->load_len - 1) % HISTORY] : 0.0;
}

static void tick(void *ctx, pnl_app_t *app)
{
    struct dashboard *dash = ctx;
    char line[80];
    size_t s;

    (void)app;
    dash->tick++;
    for (s = 0; s < SERVICES; s++)
        dash->load[s][dash->load_len % HISTORY] = sample(dash->tick, s);
    dash->load_len++;
    for (s = 0; s < SERVICES; s++) {
        if (current(dash, s) > 85.0) {
            snprintf(line, sizeof(line), "tick %d: %s above 85%%", dash->tick, names[s]);
            add_log(dash, line);
        }
    }
}

static void draw_services(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct dashboard *dash = ctx;
    size_t s;

    (void)app;
    for (s = 0; s < SERVICES; s++) {
        double load = current(dash, s);
        int y = (int)s;
        int style = s == dash->selected && pnl_canvas_focused(canvas) ? PNL_BOLD : 0;

        pnl_canvas_text(canvas, 0, y, PNL_FG(load > 85.0 ? PNL_ROLE_FAIL : PNL_ROLE_OK), load > 85.0 ? "!!" : "ok");
        pnl_canvas_text(canvas, 3, y, style, names[s]);
        pnl_canvas_bar(canvas, 13, y, pnl_canvas_width(canvas) - 14, load / 100.0, PNL_METER_LEVEL);
    }
}

static int services_key(void *ctx, pnl_app_t *app, int key)
{
    struct dashboard *dash = ctx;
    char line[64];
    int yes = 0;

    if (key == PNL_KEY_UP || key == 'k')
        dash->selected = (dash->selected + SERVICES - 1) % SERVICES;
    else if (key == PNL_KEY_DOWN || key == 'j')
        dash->selected = (dash->selected + 1) % SERVICES;
    else if (key != PNL_KEY_ENTER)
        return 0;
    if (key != PNL_KEY_ENTER)
        return 1;
    snprintf(line, sizeof(line), "restart %s?", names[dash->selected]);
    if (pnl_confirm(app, line, &yes) == PNL_OK && yes) {
        snprintf(line, sizeof(line), "%s restarted", names[dash->selected]);
        pnl_toast(app, PNL_MARK_OK, line);
    }
    return 1;
}

static void draw_load(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct dashboard *dash = ctx;
    double ordered[HISTORY];
    size_t count = dash->load_len < HISTORY ? dash->load_len : HISTORY;
    char line[48];
    size_t i;

    (void)app;
    for (i = 0; i < count; i++)
        ordered[i] = dash->load[dash->selected][(dash->load_len - count + i) % HISTORY];
    snprintf(line, sizeof(line), "%s  %.0f%%", names[dash->selected], current(dash, dash->selected));
    pnl_canvas_text(canvas, 0, 0, 0, line);
    pnl_canvas_sparkline(canvas, 0, 1, pnl_canvas_width(canvas), ordered, count, 100.0, PNL_METER_LEVEL);
}

static void draw_log(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct dashboard *dash = ctx;
    int rows = pnl_canvas_height(canvas);
    size_t first = dash->log_len > (size_t)rows ? dash->log_len - (size_t)rows : 0;
    size_t i;

    (void)app;
    if (dash->log_len == 0)
        pnl_canvas_text(canvas, 0, 0, PNL_FG(PNL_ROLE_MUTED), "nothing yet");
    for (i = first; i < dash->log_len; i++)
        pnl_canvas_text(canvas, 0, (int)(i - first), PNL_FG(PNL_ROLE_MUTED), dash->log[i]);
}

static void draw_about(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    (void)ctx;
    (void)app;
    pnl_canvas_text(canvas, 0, 0, 0, "four simulated services, sampled every 200 ms");
    pnl_canvas_text(canvas, 0, 1, PNL_FG(PNL_ROLE_MUTED), "narrow the terminal to see panels fold away");
}

static int build(struct dashboard *dash, pnl_layout_t *layout)
{
    int right = 0;
    int tabs = 0;
    int node = 0;
    int status = pnl_layout_panel(layout, PNL_LAYOUT_ROOT, "services", draw_services, services_key, dash, &node);

    if (status == PNL_OK)
        status = pnl_layout_size(layout, node, 0, 30);
    if (status == PNL_OK)
        status = pnl_layout_hints(layout, node, "up/down: pick a service  enter: restart");
    if (status == PNL_OK)
        status = pnl_layout_split(layout, PNL_LAYOUT_ROOT, PNL_SPLIT_ROWS, NULL, &right);
    if (status == PNL_OK)
        status = pnl_layout_size(layout, right, 0, 36);
    if (status == PNL_OK)
        status = pnl_layout_panel(layout, right, "load", draw_load, NULL, dash, &node);
    if (status == PNL_OK)
        status = pnl_layout_size(layout, node, 3, 3);
    if (status == PNL_OK)
        status = pnl_layout_split(layout, right, PNL_SPLIT_TABS, NULL, &tabs);
    if (status == PNL_OK)
        status = pnl_layout_panel(layout, tabs, "alerts", draw_log, NULL, dash, NULL);
    if (status == PNL_OK)
        status = pnl_layout_panel(layout, tabs, "about", draw_about, NULL, NULL, NULL);
    return status;
}

static void open_dashboard(void *ctx, pnl_app_t *app)
{
    pnl_layout_t *layout = NULL;
    int status = pnl_layout_create(app, PNL_SPLIT_COLUMNS, &layout);

    if (status == PNL_OK)
        status = build(ctx, layout);
    if (status == PNL_OK)
        status = pnl_view_push_layout(app, "dashboard", layout);
    pnl_layout_destroy(layout);
    if (status != PNL_OK)
        pnl_result(app, PNL_MARK_FAIL, pnl_strerror(status));
}

static void show_status(void *ctx, pnl_app_t *app)
{
    struct dashboard *dash = ctx;
    size_t s;

    for (s = 0; s < SERVICES; s++)
        pnl_status_meter(app, current(dash, s) > 85.0 ? PNL_MARK_FAIL : PNL_MARK_OK, names[s], current(dash, s) / 100.0);
}

int main(int argc, char **argv)
{
    struct dashboard dash;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int status;

    memset(&dash, 0, sizeof(dash));
    status = pnl_app_create("services", NULL, NULL, NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, show_status, &dash);
    if (status == PNL_OK)
        status = pnl_app_add_timer(app, 200, tick, &dash, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Dashboard", open_dashboard, &dash);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc, argv, &exit_code);
    pnl_app_destroy(app);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    return exit_code;
}
