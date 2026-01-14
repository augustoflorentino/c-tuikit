#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#define nap_ms(ms) Sleep(ms)
#else
static void nap_ms(long ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

struct service {
    int running;
    long started;
    int ticks;
    int ticker;
};

static void show_status(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;
    char line[64];

    pnl_status_line(app, PNL_MARK_OK, "config file found");
    if (!svc->running) {
        pnl_status_line(app, PNL_MARK_FAIL, "service stopped");
        return;
    }
    snprintf(line, sizeof(line), "service running for %lds", (long)time(NULL) - svc->started);
    pnl_status_line(app, PNL_MARK_OK, line);
}

static void start(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;

    if (svc->running) {
        pnl_result(app, PNL_MARK_WAIT, "already running");
        return;
    }
    svc->running = 1;
    svc->started = (long)time(NULL);
    pnl_result(app, PNL_MARK_OK, "service started");
}

static void stop(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;

    if (!svc->running) {
        pnl_result(app, PNL_MARK_FAIL, "the service is not running");
        return;
    }
    svc->running = 0;
    pnl_result(app, PNL_MARK_OK, "service stopped");
}

static void tick(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;

    (void)app;
    svc->ticks++;
}

static void draw_live(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct service *svc = ctx;
    char line[64];
    int width = pnl_canvas_width(canvas) - 4;
    int filled = width > 0 ? svc->ticks % (width + 1) : 0;

    (void)app;
    snprintf(line, sizeof(line), "%d ticks of 100 ms", svc->ticks);
    pnl_canvas_text(canvas, 2, 0, 0, line);
    pnl_canvas_fill(canvas, 2, 2, filled, 1, PNL_BG(PNL_COLOR_CYAN));
}

static int live_key(void *ctx, pnl_app_t *app, int key)
{
    struct service *svc = ctx;

    if (key != 'q' && key != PNL_KEY_ESCAPE)
        return 0;
    pnl_app_remove_timer(app, svc->ticker);
    pnl_view_pop(app);
    return 1;
}

static void watch_live(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;

    svc->ticks = 0;
    if (pnl_app_add_timer(app, 100, tick, svc, &svc->ticker) == PNL_OK)
        pnl_view_push(app, "live", draw_live, live_key, svc);
}

static void copy_files(void *ctx, pnl_task_t *task)
{
    static const int total = 40;
    char line[64];
    int i;

    (void)ctx;
    pnl_task_step(task, "copying files");
    for (i = 1; i <= total; i++) {
        if (pnl_task_cancelled(task)) {
            pnl_task_result(task, PNL_MARK_WAIT, "backup cancelled, partial copy kept");
            return;
        }
        nap_ms(50);
        snprintf(line, sizeof(line), "copied file-%02d.dat", i);
        pnl_task_log(task, line);
        pnl_task_progress(task, (unsigned long long)i, (unsigned long long)total);
    }
    pnl_task_result(task, PNL_MARK_OK, "backup finished: 40 files");
}

static void backup(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_task_run(app, "backup", copy_files, NULL);
}

static void print_lines(void *ctx)
{
    (void)ctx;
    printf("plain output works here: the terminal is back to normal\n");
    printf("press enter to return");
    fflush(stdout);
    getchar();
}

static void print_something(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    pnl_app_with_terminal(app, print_lines, NULL);
}

int main(int argc, char **argv)
{
    struct service svc = {0, 0, 0, 0};
    int exit_code = 1;
    pnl_app_t *app = NULL;
    int status = pnl_app_create("service - demo", NULL, NULL, NULL, &app);

    if (status == PNL_OK)
        status = pnl_app_set_status(app, show_status, &svc, 1000);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Start", start, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Stop", stop, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Watch live", watch_live, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Back up", backup, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Print something", print_something, NULL);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc, argv, &exit_code);
    pnl_app_destroy(app);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    return exit_code;
}
