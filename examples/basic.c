#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <time.h>

struct service {
    int running;
    long started;
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

static void say_hello(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
    printf("plain output works here: the terminal is back to normal\n");
    printf("press enter to return");
    fflush(stdout);
    getchar();
}

int main(void)
{
    struct service svc = {0, 0};
    pnl_app_t *app = NULL;
    int status = pnl_app_create("service - demo", NULL, NULL, NULL, &app);

    if (status == PNL_OK)
        status = pnl_app_set_status(app, show_status, &svc, 1000);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Start", start, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Stop", stop, &svc);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Print something", say_hello, NULL);
    if (status == PNL_OK)
        status = pnl_app_run(app);
    pnl_app_destroy(app);
    if (status != PNL_OK) {
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
        return 1;
    }
    return 0;
}
