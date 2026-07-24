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

#define RELEASES_MAX 16

static const char *const environments[] = {"staging", "production", "preview"};

struct release {
    char version[16];
    char environment[16];
    char when[16];
    char status[16];
};

struct deployer {
    size_t environment;
    int next_version;
    struct release releases[RELEASES_MAX];
    size_t releases_len;
    int last_failed;
    int deploy_failed;
    pnl_widget_t *flow;
    pnl_widget_t *scene;
    pnl_widget_t *stage;
    pnl_widget_t *gap;
};

#define SERVER_TOP "            .------.\n"
#define SERVER_BOTTOM "            | ==== |\n            '------'"

static const char shipping[] = "[#]         .------.\n"
                               "            | =  = |\n" SERVER_BOTTOM "\f"
                               "   [#]      .------.\n"
                               "            | == = |\n" SERVER_BOTTOM "\f"
                               "      [#]   .------.\n"
                               "            | = == |\n" SERVER_BOTTOM "\f"
                               "         [#].------.\n"
                               "            | ==== |\n" SERVER_BOTTOM;

static const char shipped[] = SERVER_TOP "    \\o/     | ==== |   live\n" SERVER_BOTTOM;

static const char refused[] = SERVER_TOP "    [#]  x  | ?? ? |   previous release kept\n" SERVER_BOTTOM;

static void show_status(void *ctx, pnl_app_t *app)
{
    struct deployer *dep = ctx;
    char line[96];

    pnl_status_line(app, PNL_MARK_OK, "target", environments[dep->environment]);
    if (dep->releases_len == 0) {
        pnl_status_line(app, PNL_MARK_WAIT, "release", "nothing deployed yet");
        return;
    }
    snprintf(line, sizeof(line), "%s %s", dep->releases[dep->releases_len - 1].version,
             dep->last_failed ? "failed" : "live");
    pnl_status_line(app, dep->last_failed ? PNL_MARK_FAIL : PNL_MARK_OK, "release", line);
}

static void record(struct deployer *dep, int failed)
{
    struct release *rel;
    time_t now = time(NULL);

    if (dep->releases_len == RELEASES_MAX) {
        memmove(dep->releases, dep->releases + 1, sizeof(dep->releases) - sizeof(dep->releases[0]));
        dep->releases_len--;
    }
    rel = &dep->releases[dep->releases_len++];
    snprintf(rel->version, sizeof(rel->version), "1.%d.0", dep->next_version++);
    snprintf(rel->environment, sizeof(rel->environment), "%s", environments[dep->environment]);
    strftime(rel->when, sizeof(rel->when), "%H:%M:%S", localtime(&now));
    snprintf(rel->status, sizeof(rel->status), "%s", failed ? "failed" : "live");
    dep->last_failed = failed;
}

static int run_step(struct deployer *dep, pnl_task_t *task, int index, const char *step, int lines)
{
    char line[96];
    int i;

    pnl_task_step(task, step);
    pnl_widget_set_step(dep->flow, index, PNL_STEP_RUN, "running");
    for (i = 1; i <= lines; i++) {
        if (pnl_task_cancelled(task)) {
            pnl_widget_set_step(dep->flow, index, PNL_STEP_FAIL, "cancelled");
            return 0;
        }
        nap_ms(60);
        snprintf(line, sizeof(line), "%s: %d/%d", step, i, lines);
        pnl_task_log(task, line);
        if (index == 1 && i == 9 && dep->next_version % 3 == 0) {
            pnl_widget_set_step(dep->flow, index, PNL_STEP_FAIL, "2 failed");
            return 0;
        }
    }
    snprintf(line, sizeof(line), "%d.%ds", lines * 6 / 100, lines * 6 % 100 / 10);
    pnl_widget_set_step(dep->flow, index, PNL_STEP_OK, line);
    return 1;
}

static void finish(struct deployer *dep, int from, int state)
{
    int i;

    for (i = from; i < 4; i++)
        pnl_widget_set_step(dep->flow, i, PNL_STEP_SKIP, "");
    pnl_widget_set_value(dep->scene, state);
}

static void deploy_done(void *ctx, pnl_app_t *app)
{
    struct deployer *dep = ctx;

    (void)app;
    record(dep, dep->deploy_failed);
}

static void deploy_work(void *ctx, pnl_task_t *task)
{
    static const char *const steps[] = {"build", "test", "upload", "switch traffic"};
    static const int lines[] = {8, 12, 6, 4};
    struct deployer *dep = ctx;
    size_t i;

    pnl_task_then(task, deploy_done, dep);
    pnl_task_show(task, dep->stage);
    for (i = 0; i < 4; i++)
        pnl_widget_set_step(dep->flow, (int)i, PNL_STEP_WAIT, "");
    pnl_widget_set_value(dep->scene, PNL_STEP_RUN);
    dep->deploy_failed = 1;
    for (i = 0; i < 4; i++) {
        pnl_task_progress(task, i, 4);
        if (!run_step(dep, task, (int)i, steps[i], lines[i])) {
            finish(dep, (int)i + 1, PNL_STEP_FAIL);
            pnl_task_result(task, PNL_MARK_FAIL, pnl_task_cancelled(task) ? "deploy cancelled, previous release still live"
                                                                           : "tests failed, previous release still live");
            return;
        }
    }
    pnl_task_progress(task, 4, 4);
    finish(dep, 4, PNL_STEP_OK);
    dep->deploy_failed = 0;
    pnl_task_result(task, PNL_MARK_OK, "deployed");
}

static void deploy(void *ctx, pnl_app_t *app)
{
    struct deployer *dep = ctx;
    char question[96];
    int yes = 0;

    snprintf(question, sizeof(question), "deploy to %s?", environments[dep->environment]);
    if (pnl_confirm(app, question, &yes) == PNL_OK && yes)
        pnl_task_run(app, "deploy", deploy_work, dep);
}

static void choose_target(void *ctx, pnl_app_t *app)
{
    struct deployer *dep = ctx;
    size_t index = 0;
    int ok = 0;

    if (pnl_pick(app, "target", environments, 3, &index, &ok) == PNL_OK && ok) {
        dep->environment = index;
        pnl_result(app, PNL_MARK_OK, "target changed");
    }
}

static void releases(void *ctx, pnl_app_t *app)
{
    static const char *const headers[] = {"version", "environment", "time", "status"};
    static const int flags[] = {0, PNL_COLUMN_OPTIONAL, PNL_COLUMN_OPTIONAL, 0};
    struct deployer *dep = ctx;
    const char *cells[RELEASES_MAX * 4];
    size_t i;

    for (i = 0; i < dep->releases_len; i++) {
        cells[i * 4] = dep->releases[i].version;
        cells[i * 4 + 1] = dep->releases[i].environment;
        cells[i * 4 + 2] = dep->releases[i].when;
        cells[i * 4 + 3] = dep->releases[i].status;
    }
    pnl_table(app, "releases", headers, flags, 4, cells, dep->releases_len);
}

static void build_log(void *ctx, pnl_app_t *app)
{
    char text[4096];
    size_t len = 0;
    int i;

    (void)ctx;
    for (i = 1; i <= 60 && len < sizeof(text) - 64; i++)
        len += (size_t)snprintf(text + len, sizeof(text) - len, "[%02d] compiling module_%02d.c%s\n", i, i,
                                i % 23 == 0 ? "  warning: unused variable" : "");
    pnl_pager(app, "build log", text, len);
}

static int build_stage(pnl_app_t *app, struct deployer *dep)
{
    static const char *const steps[] = {"build", "test", "upload", "switch traffic"};
    int status = pnl_widget_flow(app, &dep->flow);
    int i;

    for (i = 0; i < 4 && status == PNL_OK; i++)
        status = pnl_widget_add_step(dep->flow, steps[i], NULL);
    if (status == PNL_OK)
        status = pnl_widget_scene(app, &dep->scene);
    if (status == PNL_OK)
        status = pnl_widget_add_scene(dep->scene, PNL_STEP_RUN, shipping, PNL_FG(PNL_ROLE_ACCENT), 0, 180);
    if (status == PNL_OK)
        status = pnl_widget_add_scene(dep->scene, PNL_STEP_OK, shipped, PNL_FG(PNL_ROLE_OK), 0, 0);
    if (status == PNL_OK)
        status = pnl_widget_add_scene(dep->scene, PNL_STEP_FAIL, refused, PNL_FG(PNL_ROLE_FAIL), 0, 0);
    if (status == PNL_OK)
        status = pnl_widget_stack(app, PNL_STACK_ROWS, &dep->stage);
    if (status == PNL_OK)
        status = pnl_widget_add(dep->stage, dep->flow, 0);
    if (status == PNL_OK)
        status = pnl_widget_text(app, "", 0, &dep->gap);
    if (status == PNL_OK)
        status = pnl_widget_add(dep->stage, dep->gap, 0);
    if (status == PNL_OK)
        status = pnl_widget_add(dep->stage, dep->scene, 0);
    if (status == PNL_OK)
        status = pnl_app_set_banner_widget(app, dep->flow);
    return status;
}


int main(int argc, char **argv)
{
    struct deployer dep;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int status;

    memset(&dep, 0, sizeof(dep));
    dep.next_version = 4;
    status = pnl_app_create("deploy", NULL, &app);
    if (status == PNL_OK)
        status = build_stage(app, &dep);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, show_status, &dep);
    if (status == PNL_OK)
        status = pnl_app_set_refresh(app, 1000);
    if (status == PNL_OK)
        status = pnl_app_set_memory(app, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Deploy", deploy, &dep);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Target", choose_target, &dep);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Releases", releases, &dep);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Build log", build_log, NULL);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc, argv, &exit_code);
    pnl_widget_destroy(dep.stage);
    pnl_widget_destroy(dep.flow);
    pnl_widget_destroy(dep.scene);
    pnl_widget_destroy(dep.gap);
    pnl_app_destroy(app);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    return exit_code;
}
