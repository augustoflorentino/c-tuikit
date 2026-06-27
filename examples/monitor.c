#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <string.h>
#include <time.h>


struct clock {
    int style;
};

struct machine {
    pnl_widget_t *cpu;
    pnl_widget_t *memory;
    pnl_widget_t *disk;
    pnl_widget_t *network;
    pnl_widget_t *load;
    pnl_widget_t *latency;
    pnl_widget_t *status;
    pnl_widget_t *title;
    pnl_log_t *events;
    unsigned int seed;
    int ticks;
};

static void clock_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct clock *clock = ctx;
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char text[16];

    (void)app;
    if (local == NULL)
        return;
    snprintf(text, sizeof(text), "%02d:%02d:%02d", local->tm_hour, local->tm_min, local->tm_sec);
    pnl_canvas_big(canvas, 0, 0, PNL_BIG_SMALL, PNL_FG(clock->style), 0, text);
}

static int clock_height(void *ctx, pnl_app_t *app, int width)
{
    (void)ctx;
    (void)app;
    (void)width;
    return pnl_big_height(PNL_BIG_SMALL) + 1;
}

static int clock_key(void *ctx, pnl_app_t *app, int key)
{
    struct clock *clock = ctx;

    (void)app;
    if (key != 'c')
        return 0;
    clock->style = clock->style == PNL_ROLE_ACCENT ? PNL_ROLE_OK : PNL_ROLE_ACCENT;
    return 1;
}

static const char *clock_hints(void *ctx)
{
    (void)ctx;
    return "c: clock color";
}

static unsigned int roll(struct machine *machine)
{
    machine->seed = machine->seed * 1103515245u + 12345u;
    return machine->seed >> 16;
}

static double wander(struct machine *machine, double from, double step)
{
    double next = from + ((double)(roll(machine) % 2001) / 1000.0 - 1.0) * step;

    return next < 0.0 ? 0.0 : next > 1.0 ? 1.0 : next;
}

static void sample(void *ctx, pnl_app_t *app)
{
    static double cpu = 0.4;
    static double memory = 0.55;
    static double disk = 0.7;
    struct machine *machine = ctx;
    char line[96];

    (void)app;
    machine->ticks++;
    cpu = wander(machine, cpu, 0.15);
    memory = wander(machine, memory, 0.03);
    disk = wander(machine, disk, 0.005);
    pnl_widget_set_value(machine->cpu, cpu);
    pnl_widget_set_value(machine->memory, memory);
    pnl_widget_set_value(machine->disk, disk);
    pnl_widget_set_value(machine->network, (double)(roll(machine) % 100));
    pnl_widget_set_value(machine->load, cpu * 100.0);
    pnl_widget_set_value(machine->latency, 20.0 + 15.0 * cpu + (double)(roll(machine) % 10));
    snprintf(line, sizeof(line), "%d samples taken", machine->ticks);
    pnl_widget_set_text(machine->status, line);
    if (machine->ticks % 8 == 0) {
        snprintf(line, sizeof(line), "cpu at %.0f%%, memory at %.0f%%", cpu * 100.0, memory * 100.0);
        pnl_log_write(machine->events, cpu > 0.8 ? PNL_LEVEL_WARN : PNL_LEVEL_INFO, line);
    }
}

static int build_left(pnl_app_t *app, struct machine *machine, pnl_widget_t *clock, pnl_widget_t **out_left)
{
    pnl_widget_t *left = NULL;
    int status = pnl_widget_stack(app, PNL_STACK_ROWS, &left);

    if (status == PNL_OK)
        status = pnl_widget_text(app, "local machine\n", PNL_BOLD, &machine->title);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->title, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, clock, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->cpu, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->memory, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->disk, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->network, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(left, machine->status, 0);
    *out_left = left;
    return status;
}

static int create_widgets(pnl_app_t *app, struct machine *machine)
{
    int status = pnl_widget_bar(app, "cpu    ", PNL_METER_LEVEL | PNL_METER_PERCENT, &machine->cpu);

    if (status == PNL_OK)
        status = pnl_widget_bar(app, "memory ", PNL_METER_LEVEL | PNL_METER_PERCENT, &machine->memory);
    if (status == PNL_OK)
        status = pnl_widget_bar(app, "disk   ", PNL_METER_LEVEL | PNL_METER_PERCENT, &machine->disk);
    if (status == PNL_OK)
        status = pnl_widget_graph(app, "network", PNL_GRAPH_SPARKLINE, 1, PNL_METER_ACCENT, 200, &machine->network);
    if (status == PNL_OK)
        status = pnl_widget_graph(app, NULL, PNL_GRAPH_CHART, 0, PNL_METER_LEVEL, 400, &machine->load);
    if (status == PNL_OK)
        status = pnl_widget_graph(app, NULL, PNL_GRAPH_PLOT, 0, PNL_METER_ACCENT, 400, &machine->latency);
    if (status == PNL_OK)
        status = pnl_widget_spinner(app, PNL_SPINNER_ARC, "starting", &machine->status);
    if (status == PNL_OK)
        status = pnl_widget_set_range(machine->load, 0.0, 100.0);
    return status;
}

static int build_layout(pnl_app_t *app, struct machine *machine, pnl_widget_t *left, pnl_widget_t *events,
                        pnl_layout_t **out_layout)
{
    pnl_layout_t *layout = NULL;
    int right = 0;
    int node = 0;
    int status = pnl_layout_create(app, PNL_SPLIT_COLUMNS, &layout);

    if (status == PNL_OK)
        status = pnl_layout_widget(layout, PNL_LAYOUT_ROOT, "machine", left, &node);
    if (status == PNL_OK)
        status = pnl_layout_size(layout, node, 34, 30);
    if (status == PNL_OK)
        status = pnl_layout_split(layout, PNL_LAYOUT_ROOT, PNL_SPLIT_ROWS, NULL, &right);
    if (status == PNL_OK)
        status = pnl_layout_widget(layout, right, "load", machine->load, NULL);
    if (status == PNL_OK)
        status = pnl_layout_widget(layout, right, "latency", machine->latency, NULL);
    if (status == PNL_OK)
        status = pnl_layout_widget(layout, right, "events", events, NULL);
    *out_layout = layout;
    return status;
}

struct screen {
    pnl_layout_t *layout;
};

static void open_monitor(void *ctx, pnl_app_t *app)
{
    struct screen *screen = ctx;

    pnl_view_push_layout(app, "monitor", screen->layout);
}

int main(int argc, char **argv)
{
    static const pnl_widget_class_t clock_class = {clock_draw, clock_key, clock_height, clock_hints, NULL};
    struct clock clock_state = {PNL_ROLE_ACCENT};
    struct machine machine;
    struct screen screen = {NULL};
    pnl_widget_t *clock = NULL;
    pnl_widget_t *left = NULL;
    pnl_widget_t *events = NULL;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int status;

    memset(&machine, 0, sizeof(machine));
    machine.seed = (unsigned int)time(NULL);
    status = pnl_app_create("monitor", NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_enable_motion(app, PNL_MOTION_SLIDE | PNL_MOTION_ENTER | PNL_MOTION_COUNT);
    if (status == PNL_OK)
        status = pnl_log_create(app, 500, &machine.events);
    if (status == PNL_OK)
        status = create_widgets(app, &machine);
    if (status == PNL_OK)
        status = pnl_widget_create(app, &clock_class, &clock_state, &clock);
    if (status == PNL_OK)
        status = pnl_widget_log(app, machine.events, &events);
    if (status == PNL_OK)
        status = build_left(app, &machine, clock, &left);
    if (status == PNL_OK)
        status = build_layout(app, &machine, left, events, &screen.layout);
    if (status == PNL_OK)
        status = pnl_app_add_timer(app, 250, sample, &machine, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Monitor", open_monitor, &screen);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc, argv, &exit_code);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    pnl_layout_destroy(screen.layout);
    pnl_widget_destroy(left);
    pnl_widget_destroy(events);
    pnl_widget_destroy(clock);
    pnl_widget_destroy(machine.cpu);
    pnl_widget_destroy(machine.memory);
    pnl_widget_destroy(machine.disk);
    pnl_widget_destroy(machine.network);
    pnl_widget_destroy(machine.load);
    pnl_widget_destroy(machine.latency);
    pnl_widget_destroy(machine.status);
    pnl_widget_destroy(machine.title);
    pnl_log_destroy(machine.events);
    pnl_app_destroy(app);
    return exit_code;
}
