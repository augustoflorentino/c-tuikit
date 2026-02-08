#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_TASK_TEXT 256
#define PNL_TASK_LOG 100
#define PNL_TASK_BAR 30
#define PNL_FOOTER_RUNNING "esc: cancel"
#define PNL_FOOTER_CANCELLING "cancelling..."
#define PNL_FOOTER_DONE "any key: back"

struct pnl_task {
    pnl_app_t *app;
    pnl_work_fn fn;
    void *ctx;
    struct pnl_thread thread;
    struct pnl_lock lock;
    int threaded;
    int joined;
    pnl_action_fn then_fn;
    void *then_ctx;
    long long started_ms;
    const char *footer;
    unsigned long long done;
    unsigned long long total;
    int cancel;
    int finished;
    int result_set;
    int result_mark;
    char step[PNL_TASK_TEXT];
    char result[PNL_TASK_TEXT];
    char log[PNL_TASK_LOG][PNL_TASK_TEXT];
    size_t log_start;
    size_t log_len;
};

static void pnl_task_copy(char *dest, const char *text)
{
    size_t len = strlen(text);

    if (len >= PNL_TASK_TEXT) {
        len = PNL_TASK_TEXT - 1;
        while (len > 0 && ((unsigned char)text[len] & 0xc0) == 0x80)
            len--;
    }
    memcpy(dest, text, len);
    dest[len] = '\0';
}

static int pnl_task_cli(const pnl_task_t *task)
{
    return !task->threaded;
}

void pnl_task_progress(pnl_task_t *task, unsigned long long done, unsigned long long total)
{
    if (task == NULL)
        return;
    if (pnl_task_cli(task)) {
        task->done = done;
        task->total = total;
        pnl_cli_progress(task->step, done, total);
        return;
    }
    pnl_lock(&task->lock);
    task->done = done;
    task->total = total;
    pnl_unlock(&task->lock);
}

int pnl_task_step(pnl_task_t *task, const char *text)
{
    if (task == NULL || text == NULL)
        return PNL_ERR_ARG;
    if (pnl_task_cli(task)) {
        pnl_task_copy(task->step, text);
        pnl_cli_progress(task->step, task->done, task->total);
        return PNL_OK;
    }
    pnl_lock(&task->lock);
    pnl_task_copy(task->step, text);
    pnl_unlock(&task->lock);
    return PNL_OK;
}

int pnl_task_log(pnl_task_t *task, const char *line)
{
    size_t slot;

    if (task == NULL || line == NULL)
        return PNL_ERR_ARG;
    if (pnl_task_cli(task)) {
        pnl_cli_log(task->app, line);
        pnl_cli_progress(task->step, task->done, task->total);
        return PNL_OK;
    }
    pnl_lock(&task->lock);
    slot = (task->log_start + task->log_len) % PNL_TASK_LOG;
    pnl_task_copy(task->log[slot], line);
    if (task->log_len < PNL_TASK_LOG)
        task->log_len++;
    else
        task->log_start = (task->log_start + 1) % PNL_TASK_LOG;
    pnl_unlock(&task->lock);
    return PNL_OK;
}

int pnl_task_cancelled(pnl_task_t *task)
{
    int cancel;

    if (task == NULL || pnl_task_cli(task))
        return 0;
    pnl_lock(&task->lock);
    cancel = task->cancel;
    pnl_unlock(&task->lock);
    return cancel;
}

int pnl_task_result(pnl_task_t *task, int mark, const char *text)
{
    if (task == NULL || text == NULL || !pnl_valid_mark(mark))
        return PNL_ERR_ARG;
    if (pnl_task_cli(task)) {
        if (pnl_term_stderr_is_tty())
            fputs("\r\x1b[K", stderr);
        return pnl_cli_result(task->app, mark, text);
    }
    pnl_lock(&task->lock);
    pnl_task_copy(task->result, text);
    task->result_mark = mark;
    task->result_set = 1;
    pnl_unlock(&task->lock);
    return PNL_OK;
}

int pnl_task_then(pnl_task_t *task, pnl_action_fn then_fn, void *then_ctx)
{
    if (task == NULL || then_fn == NULL)
        return PNL_ERR_ARG;
    task->then_fn = then_fn;
    task->then_ctx = then_ctx;
    return PNL_OK;
}

static void pnl_task_main(void *arg)
{
    pnl_task_t *task = arg;

    task->fn(task->ctx, task);
    pnl_lock(&task->lock);
    task->finished = 1;
    pnl_unlock(&task->lock);
}

static void pnl_task_join(pnl_task_t *task)
{
    if (task->joined)
        return;
    pnl_thread_join(&task->thread);
    task->joined = 1;
    task->app->tasks_running--;
    task->footer = PNL_FOOTER_DONE;
    if (task->then_fn != NULL)
        task->then_fn(task->then_ctx, task->app);
}

static void pnl_task_draw_progress(pnl_task_t *task, pnl_canvas_t *canvas, int y)
{
    char line[96];
    int filled;

    if (task->total == 0) {
        snprintf(line, sizeof(line), "%llds", (pnl_term_now_ms() - task->started_ms) / 1000);
        pnl_canvas_text(canvas, 2, y, PNL_FG(PNL_COLOR_GRAY), line);
        return;
    }
    filled = (int)((task->done >= task->total ? task->total : task->done) * PNL_TASK_BAR / task->total);
    pnl_canvas_fill(canvas, 2, y, filled, 1, PNL_BG(PNL_COLOR_CYAN));
    pnl_canvas_fill(canvas, 2 + filled, y, PNL_TASK_BAR - filled, 1, PNL_BG(PNL_COLOR_GRAY));
    snprintf(line, sizeof(line), "%3d%%  %llu / %llu", (int)(task->done >= task->total ? 100 : task->done * 100 / task->total),
             task->done, task->total);
    pnl_canvas_text(canvas, 3 + PNL_TASK_BAR, y, 0, line);
}

static void pnl_task_draw_log(pnl_task_t *task, pnl_canvas_t *canvas, int y)
{
    int rows = pnl_canvas_height(canvas) - y;
    size_t shown = rows > 0 ? (size_t)rows : 0;
    size_t first;
    size_t i;

    if (shown > task->log_len)
        shown = task->log_len;
    first = task->log_len - shown;
    for (i = 0; i < shown; i++) {
        const char *line = task->log[(task->log_start + first + i) % PNL_TASK_LOG];

        pnl_canvas_text(canvas, 2, y + (int)i, PNL_FG(PNL_COLOR_GRAY), line);
    }
}

static void pnl_task_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    pnl_task_t *task = ctx;
    int finished;

    (void)app;
    pnl_lock(&task->lock);
    finished = task->finished;
    pnl_canvas_text(canvas, 2, 0, PNL_BOLD, task->step[0] ? task->step : "working");
    if (finished && task->result_set)
        pnl_draw_mark(canvas, 2, task->result_mark, task->result);
    else if (finished)
        pnl_draw_mark(canvas, 2, PNL_MARK_OK, task->cancel ? "cancelled" : "finished");
    else
        pnl_task_draw_progress(task, canvas, 2);
    pnl_task_draw_log(task, canvas, 4);
    pnl_unlock(&task->lock);
    if (finished)
        pnl_task_join(task);
}

static int pnl_task_key(void *ctx, pnl_app_t *app, int key)
{
    pnl_task_t *task = ctx;

    if (task->joined) {
        pnl_view_pop(app);
        return 1;
    }
    if (key != PNL_KEY_ESCAPE && key != 'q')
        return 1;
    pnl_lock(&task->lock);
    task->cancel = 1;
    pnl_unlock(&task->lock);
    task->footer = PNL_FOOTER_CANCELLING;
    return 1;
}

static void pnl_task_free(pnl_app_t *app, pnl_task_t *task)
{
    if (task->threaded)
        pnl_lock_destroy(&task->lock);
    pnl_mem_free(&app->mem, task);
}

static void pnl_task_drop(pnl_app_t *app, void *ctx)
{
    pnl_task_t *task = ctx;

    pnl_lock(&task->lock);
    task->cancel = 1;
    pnl_unlock(&task->lock);
    pnl_task_join(task);
    pnl_task_free(app, task);
}

static int pnl_task_cli_run(pnl_app_t *app, pnl_task_t *task)
{
    task->fn(task->ctx, task);
    if (task->then_fn != NULL)
        task->then_fn(task->then_ctx, app);
    if (pnl_term_stderr_is_tty())
        fputs("\r\x1b[K", stderr);
    fflush(stdout);
    pnl_task_free(app, task);
    return PNL_OK;
}

static int pnl_task_start(pnl_app_t *app, pnl_task_t *task, const char *title)
{
    struct pnl_view view;
    int status;

    memset(&view, 0, sizeof(view));
    view.footer_ref = &task->footer;
    view.draw_fn = pnl_task_draw;
    view.key_fn = pnl_task_key;
    view.ctx = task;
    view.title = pnl_mem_copy(&app->mem, title);
    if (view.title == NULL)
        return PNL_ERR_NOMEM;
    status = pnl_view_push_internal(app, &view);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, view.title);
        return status;
    }
    status = pnl_thread_start(&task->thread, pnl_task_main, task);
    if (status != PNL_OK) {
        app->views_len--;
        pnl_mem_free(&app->mem, view.title);
        return status;
    }
    app->views[app->views_len - 1].drop_fn = pnl_task_drop;
    app->tasks_running++;
    return PNL_OK;
}

int pnl_task_run(pnl_app_t *app, const char *title, pnl_work_fn work_fn, void *work_ctx)
{
    pnl_task_t *task;
    int status;

    if (app == NULL || title == NULL || work_fn == NULL || (!app->running && !app->cli))
        return PNL_ERR_ARG;
    task = pnl_mem_alloc(&app->mem, sizeof(*task));
    if (task == NULL)
        return PNL_ERR_NOMEM;
    memset(task, 0, sizeof(*task));
    task->app = app;
    task->fn = work_fn;
    task->ctx = work_ctx;
    task->started_ms = pnl_term_now_ms();
    task->footer = PNL_FOOTER_RUNNING;
    if (app->cli)
        return pnl_task_cli_run(app, task);
    status = pnl_lock_init(&task->lock);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, task);
        return status;
    }
    task->threaded = 1;
    status = pnl_task_start(app, task, title);
    if (status != PNL_OK)
        pnl_task_free(app, task);
    return status;
}
