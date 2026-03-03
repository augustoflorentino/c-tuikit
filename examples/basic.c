#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <string.h>
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

#define HISTORY 60

struct service {
    int running;
    long started;
    int ticks;
    int ticker;
    double load[HISTORY];
    size_t load_len;
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
    pnl_status_meter(app, PNL_MARK_OK, "memory", (double)((time(NULL) - svc->started) % 60) / 60.0);
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
    int i = svc->ticks % HISTORY;

    (void)app;
    svc->ticks++;
    svc->load[i] = (double)((svc->ticks * 37) % 100) * (0.5 + 0.5 * (double)(svc->ticks % 20) / 20.0);
    svc->load_len = svc->load_len < HISTORY ? svc->load_len + 1 : HISTORY;
}

static void draw_live(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct service *svc = ctx;
    char line[64];
    int width = pnl_canvas_width(canvas) - 4;
    int filled = width > 0 ? svc->ticks % (width + 1) : 0;

    (void)app;
    double ordered[HISTORY];
    size_t i;

    snprintf(line, sizeof(line), "%d ticks of 100 ms", svc->ticks);
    pnl_canvas_text(canvas, 2, 0, 0, line);
    pnl_canvas_fill(canvas, 2, 2, filled, 1, PNL_BG(PNL_ROLE_ACCENT));
    for (i = 0; i < svc->load_len; i++)
        ordered[i] = svc->load[(svc->ticks - (int)svc->load_len + (int)i) % HISTORY];
    pnl_canvas_text(canvas, 2, 4, PNL_FG(PNL_ROLE_MUTED), "load");
    pnl_canvas_sparkline(canvas, 8, 4, width - 6, ordered, svc->load_len, 100.0, PNL_METER_LEVEL);
    pnl_canvas_text(canvas, 2, 6, PNL_FG(PNL_ROLE_MUTED), "now");
    pnl_canvas_bar(canvas, 8, 6, 30, svc->load_len ? ordered[svc->load_len - 1] / 100.0 : 0.0, PNL_METER_LEVEL);
}

static int live_key(void *ctx, pnl_app_t *app, int key)
{
    struct service *svc = ctx;

    if (key == 'r') {
        svc->ticks = 0;
        return 1;
    }
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
    if (pnl_app_add_timer(app, 100, tick, svc, &svc->ticker) != PNL_OK)
        return;
    if (pnl_view_push(app, "live", draw_live, live_key, svc) == PNL_OK)
        pnl_view_set_hints(app, "r: reset the counter  esc: back");
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

static void restart(void *ctx, pnl_app_t *app)
{
    struct service *svc = ctx;
    int yes = 0;

    if (pnl_confirm(app, "restart the service now?", &yes) != PNL_OK || !yes)
        return;
    svc->running = 1;
    svc->started = (long)time(NULL);
    pnl_result(app, PNL_MARK_OK, "service restarted");
}

static void rename_service(void *ctx, pnl_app_t *app)
{
    char name[64] = "demo";
    char line[96];
    int ok = 0;

    (void)ctx;
    if (pnl_input(app, "new name for the service", name, sizeof(name), &ok) != PNL_OK || !ok)
        return;
    snprintf(line, sizeof(line), "renamed to '%s'", name);
    pnl_result(app, PNL_MARK_OK, line);
}

static void choose_profile(void *ctx, pnl_app_t *app)
{
    static const char *const profiles[] = {"development", "staging", "production", "production-eu",
                                           "production-us", "benchmark", "local"};
    char line[96];
    size_t index = 0;
    int ok = 0;

    (void)ctx;
    if (pnl_pick(app, "profile", profiles, sizeof(profiles) / sizeof(profiles[0]), &index, &ok) != PNL_OK || !ok)
        return;
    snprintf(line, sizeof(line), "profile set to %s", profiles[index]);
    pnl_result(app, PNL_MARK_OK, line);
}

static void show_log(void *ctx, pnl_app_t *app)
{
    char text[200 * 64];
    size_t len = 0;
    int i;

    (void)ctx;
    for (i = 1; i <= 200; i++) {
        len += (size_t)snprintf(text + len, sizeof(text) - len, "12:%02d:%02d\t%s request %d\n", i / 60, i % 60,
                                i % 17 == 0 ? "error" : "info ", i);
    }
    pnl_pager(app, "log", text, len);
}

static void processes(void *ctx, pnl_app_t *app)
{
    static const char *const headers[] = {"pid", "name", "cpu %", "memory"};
    static const char *const cells[] = {
        "812",  "worker",    "12.5", "220 MB", "77",   "scheduler", "0.4", "48 MB",
        "1290", "http",      "33.0", "512 MB", "4",    "init",      "0.0", "8 MB",
        "2001", "cache",     "7.1",  "1.2 GB", "1555", "indexer",   "98.2", "640 MB",
    };

    (void)ctx;
    pnl_table(app, "processes", headers, 4, cells, 6);
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
    struct service svc;
    int exit_code = 1;
    pnl_app_t *app = NULL;
    int status;

    memset(&svc, 0, sizeof(svc));
    status = pnl_app_create("service - demo", NULL, NULL, NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, show_status, &svc);
    if (status == PNL_OK)
        status = pnl_app_set_refresh(app, 1000);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Start", start, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Stop", stop, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Watch live", watch_live, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Back up", backup, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Restart", restart, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Rename", rename_service, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Choose profile", choose_profile, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Show log", show_log, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Processes", processes, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Print something", print_something, NULL);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc, argv, &exit_code);
    pnl_app_destroy(app);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    return exit_code;
}
