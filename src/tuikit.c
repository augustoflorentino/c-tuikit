#include "pnl_internal.h"

#include <stdlib.h>
#include <string.h>

static void *pnl_default_alloc(void *ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

static void pnl_default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

void *pnl_mem_alloc(const struct pnl_mem *mem, size_t size)
{
    return mem->alloc_fn(mem->ctx, size);
}

void pnl_mem_free(const struct pnl_mem *mem, void *ptr)
{
    if (ptr != NULL)
        mem->free_fn(mem->ctx, ptr);
}

char *pnl_mem_copy(const struct pnl_mem *mem, const char *text)
{
    size_t len = strlen(text) + 1;
    char *copy = pnl_mem_alloc(mem, len);

    if (copy != NULL)
        memcpy(copy, text, len);
    return copy;
}

int pnl_mem_grow(const struct pnl_mem *mem, void **items, size_t *cap, size_t needed, size_t item_size)
{
    size_t new_cap;
    void *grown;

    if (needed <= *cap)
        return PNL_OK;
    new_cap = *cap ? *cap : 8;
    while (new_cap < needed && new_cap <= (size_t)-1 / 2)
        new_cap *= 2;
    if (new_cap < needed || new_cap > (size_t)-1 / item_size)
        return PNL_ERR_NOMEM;
    grown = pnl_mem_alloc(mem, new_cap * item_size);
    if (grown == NULL)
        return PNL_ERR_NOMEM;
    if (*items != NULL) {
        memcpy(grown, *items, *cap * item_size);
        pnl_mem_free(mem, *items);
    }
    *items = grown;
    *cap = new_cap;
    return PNL_OK;
}

int pnl_env_off(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && (strcmp(value, "off") == 0 || strcmp(value, "0") == 0);
}

int pnl_app_create(const char *title, const pnl_allocator_t *allocator, pnl_app_t **out_app)
{
    struct pnl_mem mem;
    pnl_app_t *app;

    if (out_app != NULL)
        *out_app = NULL;
    if (out_app == NULL || title == NULL ||
        (allocator != NULL && (allocator->alloc_fn == NULL || allocator->free_fn == NULL)))
        return PNL_ERR_ARG;
    mem.alloc_fn = allocator != NULL ? allocator->alloc_fn : pnl_default_alloc;
    mem.free_fn = allocator != NULL ? allocator->free_fn : pnl_default_free;
    mem.ctx = allocator != NULL ? allocator->ctx : NULL;
    app = pnl_mem_alloc(&mem, sizeof(*app));
    if (app == NULL)
        return PNL_ERR_NOMEM;
    memset(app, 0, sizeof(*app));
    app->mem = mem;
    app->next_timer = 1;
    pnl_theme_choose(&app->theme, "default");
    app->title = pnl_mem_copy(&mem, title);
    if (app->title == NULL) {
        pnl_mem_free(&mem, app);
        return PNL_ERR_NOMEM;
    }
    *out_app = app;
    return PNL_OK;
}

void pnl_app_destroy(pnl_app_t *app)
{
    size_t i;

    if (app == NULL)
        return;
    pnl_term_leave(&app->term);
    pnl_views_drop(app);
    pnl_screen_free(&app->screen, &app->mem);
    for (i = 0; i < app->actions_len; i++)
        pnl_mem_free(&app->mem, app->actions[i].label);
    pnl_menu_clear_lines(app);
    pnl_memory_free(app);
    pnl_mem_free(&app->mem, app->record_path);
    pnl_mem_free(&app->mem, app->banner);
    pnl_inspect_free(app);
    pnl_mem_free(&app->mem, app->actions);
    pnl_mem_free(&app->mem, app->lines);
    pnl_mem_free(&app->mem, app->seen);
    pnl_mem_free(&app->mem, app->seen_next);
    pnl_mem_free(&app->mem, app->views);
    pnl_mem_free(&app->mem, app->values);
    pnl_term_wake_close(&app->term);
    pnl_mem_free(&app->mem, app->timers);
    pnl_mem_free(&app->mem, app->watches);
    pnl_mem_free(&app->mem, app->title);
    pnl_mem_free(&app->mem, app);
}

int pnl_app_on_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx)
{
    if (app == NULL)
        return PNL_ERR_ARG;
    app->status_fn = status_fn;
    app->status_ctx = status_ctx;
    return PNL_OK;
}

int pnl_app_set_refresh(pnl_app_t *app, int refresh_ms)
{
    if (app == NULL || refresh_ms < 0)
        return PNL_ERR_ARG;
    if (app->status_timer != PNL_STATUS_TIMER_NONE)
        pnl_app_remove_timer(app, app->status_timer);
    app->status_timer = PNL_STATUS_TIMER_NONE;
    if (refresh_ms == 0)
        return PNL_OK;
    return pnl_app_add_timer(app, refresh_ms, NULL, NULL, &app->status_timer);
}

int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx)
{
    struct pnl_action *action;
    int status;

    if (app == NULL || label == NULL || action_fn == NULL)
        return PNL_ERR_ARG;
    status = pnl_mem_grow(&app->mem, (void **)&app->actions, &app->actions_cap, app->actions_len + 1,
                          sizeof(*action));
    if (status != PNL_OK)
        return status;
    action = &app->actions[app->actions_len];
    action->label = pnl_mem_copy(&app->mem, label);
    if (action->label == NULL)
        return PNL_ERR_NOMEM;
    action->fn = action_fn;
    action->ctx = action_ctx;
    app->actions_len++;
    return PNL_OK;
}

int pnl_canvas_focused(const pnl_canvas_t *canvas)
{
    return canvas != NULL && canvas->focused;
}

int pnl_canvas_width(const pnl_canvas_t *canvas)
{
    return canvas ? canvas->width : 0;
}

int pnl_canvas_height(const pnl_canvas_t *canvas)
{
    return canvas ? canvas->height : 0;
}

int pnl_canvas_text(pnl_canvas_t *canvas, int x, int y, int style, const char *text)
{
    int end;

    if (canvas == NULL || text == NULL || x < 0 || y < 0 || x >= canvas->width || y >= canvas->height)
        return x;
    end = pnl_screen_text(canvas->screen, canvas->x + x, canvas->y + y, canvas->width - x, (uint32_t)style, text);
    return end - canvas->x;
}

int pnl_canvas_link(pnl_canvas_t *canvas, int x, int y, int style, const char *text, const char *url)
{
    int end;

    if (canvas == NULL || text == NULL || url == NULL || x < 0 || y < 0 || x >= canvas->width ||
        y >= canvas->height)
        return x;
    end = pnl_screen_link(canvas->screen, canvas->x + x, canvas->y + y, canvas->width - x, (uint32_t)style, text,
                          url[0] != '\0' ? url : NULL);
    return end - canvas->x;
}

int pnl_canvas_clip(pnl_canvas_t *canvas, int x, int y, int max, int style, const char *text)
{
    int end;

    if (canvas == NULL || text == NULL || x < 0 || y < 0 || x >= canvas->width || y >= canvas->height || max <= 0)
        return x;
    if (max > canvas->width - x)
        max = canvas->width - x;
    end = pnl_screen_text(canvas->screen, canvas->x + x, canvas->y + y, max, (uint32_t)style, text);
    return end - canvas->x;
}

void pnl_canvas_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int style)
{
    int row;

    if (canvas == NULL || x < 0 || y < 0)
        return;
    if (x + width > canvas->width)
        width = canvas->width - x;
    for (row = y; row < y + height && row < canvas->height; row++)
        pnl_screen_fill(canvas->screen, canvas->x + x, canvas->y + row, width, (uint32_t)style);
}

int pnl_text_width(const char *text)
{
    return text == NULL ? 0 : pnl_text_width_n(text, strlen(text));
}

int pnl_text_width_n(const char *text, size_t len)
{
    size_t pos = 0;
    int width = 0;

    while (pos < len) {
        uint32_t cp;
        int w;

        pos += pnl_utf8_next(text + pos, len - pos, &cp);
        w = pnl_cp_width(cp);
        width += w < 0 ? 1 : w;
    }
    return width;
}

const char *pnl_strerror(int status)
{
    switch (status) {
    case PNL_OK:
        return "ok";
    case PNL_ERR_ARG:
        return "invalid argument";
    case PNL_ERR_NOMEM:
        return "out of memory";
    case PNL_ERR_TTY:
        return "no interactive terminal";
    case PNL_ERR_IO:
        return "terminal read or write failed";
    case PNL_ERR_NOT_FOUND:
        return "not found";
    case PNL_ERR_UNSUPPORTED:
        return "not supported on this platform";
    case PNL_ERR_MISMATCH:
        return "screen differs from the snapshot";
    default:
        return "unknown status";
    }
}
