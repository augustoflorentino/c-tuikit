#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_GRAPH_CAPACITY_MAX 4096

struct pnl_text_widget {
    pnl_app_t *app;
    char *text;
    int style;
};

struct pnl_bar_widget {
    pnl_app_t *app;
    char *label;
    int flags;
    struct pnl_glide glide;
};

struct pnl_graph_widget {
    pnl_app_t *app;
    char *label;
    int kind;
    int rows;
    int flags;
    double *values;
    size_t len;
    size_t capacity;
    double min;
    double max;
};

struct pnl_spinner_widget {
    pnl_app_t *app;
    char *text;
    int kind;
};

struct pnl_stack_child {
    pnl_widget_t *widget;
    int size;
    int x;
    int y;
    int width;
    int height;
};

struct pnl_stack_widget {
    pnl_app_t *app;
    int kind;
    struct pnl_stack_child *children;
    size_t len;
    size_t cap;
    int focus;
};

int pnl_widget_create(pnl_app_t *app, const pnl_widget_class_t *widget_class, void *widget_ctx,
                      pnl_widget_t **out_widget)
{
    pnl_widget_t *widget;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || widget_class == NULL || widget_class->draw_fn == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    widget = pnl_mem_alloc(&app->mem, sizeof(*widget));
    if (widget == NULL)
        return PNL_ERR_NOMEM;
    memset(widget, 0, sizeof(*widget));
    widget->app = app;
    widget->widget_class = *widget_class;
    widget->ctx = widget_ctx;
    *out_widget = widget;
    return PNL_OK;
}

void pnl_widget_destroy(pnl_widget_t *widget)
{
    if (widget == NULL)
        return;
    if (widget->widget_class.release_fn != NULL)
        widget->widget_class.release_fn(widget->ctx);
    pnl_mem_free(&widget->app->mem, widget);
}

void *pnl_widget_ctx(const pnl_widget_t *widget)
{
    return widget != NULL ? widget->ctx : NULL;
}

void pnl_widget_paint(pnl_widget_t *widget, pnl_canvas_t *canvas)
{
    if (canvas->width > 0 && canvas->height > 0)
        widget->widget_class.draw_fn(widget->ctx, widget->app, canvas);
}

static void pnl_widget_place(pnl_widget_t *widget, pnl_canvas_t *canvas, int x, int y, int width, int height,
                             int focused)
{
    struct pnl_canvas inner = *canvas;

    inner.focused = focused;
    inner.x = canvas->x + x;
    inner.y = canvas->y + y;
    inner.width = x + width > canvas->width ? canvas->width - x : width;
    inner.height = y + height > canvas->height ? canvas->height - y : height;
    pnl_widget_paint(widget, &inner);
}

int pnl_widget_draw(pnl_widget_t *widget, pnl_canvas_t *canvas, int x, int y, int width, int height)
{
    struct pnl_canvas inner;

    if (widget == NULL || canvas == NULL || x < 0 || y < 0 || width < 0 || height < 0)
        return PNL_ERR_ARG;
    inner = *canvas;
    inner.x = canvas->x + x;
    inner.y = canvas->y + y;
    inner.width = x + width > canvas->width ? canvas->width - x : width;
    inner.height = y + height > canvas->height ? canvas->height - y : height;
    pnl_widget_paint(widget, &inner);
    return PNL_OK;
}

int pnl_widget_key(pnl_widget_t *widget, int key)
{
    if (widget == NULL || widget->widget_class.key_fn == NULL)
        return 0;
    return widget->widget_class.key_fn(widget->ctx, widget->app, key);
}

int pnl_widget_height(pnl_widget_t *widget, int width)
{
    if (widget == NULL || widget->widget_class.height_fn == NULL || width <= 0)
        return 0;
    return widget->widget_class.height_fn(widget->ctx, widget->app, width);
}

const char *pnl_widget_hints(const pnl_widget_t *widget)
{
    if (widget == NULL || widget->widget_class.hints_fn == NULL)
        return NULL;
    return widget->widget_class.hints_fn(widget->ctx);
}

static void pnl_widget_view_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    pnl_widget_t *widget = ctx;
    const char *hints = pnl_widget_hints(widget);

    (void)app;
    snprintf(widget->footer_text, sizeof(widget->footer_text), "%s%s%sesc: back", hints ? hints : "",
             hints ? "  " : "", pnl_widget_focusable(widget) ? "tab: next  " : "");
    pnl_widget_paint(widget, canvas);
}

static int pnl_widget_view_key(void *ctx, pnl_app_t *app, int key)
{
    (void)app;
    if (pnl_widget_key(ctx, key))
        return 1;
    if ((key != PNL_KEY_TAB && key != PNL_KEY_BACKTAB) || !pnl_widget_focusable(ctx))
        return 0;
    pnl_widget_focus_edge(ctx, key == PNL_KEY_BACKTAB);
    return 1;
}

int pnl_view_push_widget(pnl_app_t *app, const char *title, pnl_widget_t *widget)
{
    struct pnl_view view;
    int status;

    if (app == NULL || widget == NULL || widget->app != app)
        return PNL_ERR_ARG;
    memset(&view, 0, sizeof(view));
    view.draw_fn = pnl_widget_view_draw;
    view.key_fn = pnl_widget_view_key;
    view.ctx = widget;
    snprintf(widget->footer_text, sizeof(widget->footer_text), "esc: back");
    widget->footer = widget->footer_text;
    view.footer_ref = &widget->footer;
    if (title != NULL && (view.title = pnl_mem_copy(&app->mem, title)) == NULL)
        return PNL_ERR_NOMEM;
    status = pnl_view_push_internal(app, &view);
    if (status != PNL_OK)
        pnl_mem_free(&app->mem, view.title);
    return status;
}

int pnl_widget_builtin(pnl_app_t *app, int kind, const pnl_widget_class_t *widget_class, void *ctx,
                       pnl_widget_t **out_widget)
{
    int status = pnl_widget_create(app, widget_class, ctx, out_widget);

    if (status != PNL_OK) {
        widget_class->release_fn(ctx);
        return status;
    }
    (*out_widget)->kind = kind;
    return PNL_OK;
}

int pnl_widget_copy(pnl_app_t *app, char **slot, const char *text)
{
    char *copy = NULL;

    if (text != NULL && (copy = pnl_mem_copy(&app->mem, text)) == NULL)
        return PNL_ERR_NOMEM;
    pnl_mem_free(&app->mem, *slot);
    *slot = copy;
    app->dirty = 1;
    return PNL_OK;
}

static int pnl_lines(const char *text)
{
    int lines = 1;

    for (; text != NULL && *text != '\0'; text++)
        lines += *text == '\n';
    return text == NULL ? 0 : lines;
}

static void pnl_text_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_text_widget *widget = ctx;
    const char *line = widget->text;
    char buf[PNL_CELL_MAX];
    int y;

    (void)app;
    for (y = 0; line != NULL && y < pnl_canvas_height(canvas); y++) {
        const char *end = (strchr)(line, '\n');
        size_t len = end != NULL ? (size_t)(end - line) : strlen(line);

        snprintf(buf, sizeof(buf), "%.*s", (int)len, line);
        pnl_canvas_text(canvas, 0, y, widget->style, buf);
        line = end != NULL ? end + 1 : NULL;
    }
}

static int pnl_text_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_text_widget *widget = ctx;

    (void)app;
    (void)width;
    return pnl_lines(widget->text);
}

static void pnl_text_release(void *ctx)
{
    struct pnl_text_widget *widget = ctx;

    pnl_mem_free(&widget->app->mem, widget->text);
    pnl_mem_free(&widget->app->mem, widget);
}

int pnl_widget_text(pnl_app_t *app, const char *text, int style, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t text_class = {pnl_text_draw, NULL, pnl_text_height, NULL, pnl_text_release};
    struct pnl_text_widget *widget;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    widget = pnl_mem_alloc(&app->mem, sizeof(*widget));
    if (widget == NULL)
        return PNL_ERR_NOMEM;
    memset(widget, 0, sizeof(*widget));
    widget->app = app;
    widget->style = style;
    if (pnl_widget_copy(app, &widget->text, text != NULL ? text : "") != PNL_OK) {
        pnl_text_release(widget);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_TEXT, &text_class, widget, out_widget);
}

static void pnl_label_draw(pnl_canvas_t *canvas, const char *label, int *x)
{
    if (label == NULL)
        return;
    pnl_canvas_text(canvas, 0, 0, PNL_FG(PNL_ROLE_MUTED), label);
    *x = pnl_text_width(label) + 1;
}

static void pnl_bar_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_bar_widget *widget = ctx;
    int x = 0;

    pnl_label_draw(canvas, widget->label, &x);
    pnl_canvas_bar(canvas, x, 0, pnl_canvas_width(canvas) - x,
                   pnl_glide_at(app, &widget->glide, PNL_MOTION_COUNT), widget->flags);
}

static int pnl_one_row(void *ctx, pnl_app_t *app, int width)
{
    (void)ctx;
    (void)app;
    (void)width;
    return 1;
}

static void pnl_bar_release(void *ctx)
{
    struct pnl_bar_widget *widget = ctx;

    pnl_mem_free(&widget->app->mem, widget->label);
    pnl_mem_free(&widget->app->mem, widget);
}

int pnl_widget_bar(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t bar_class = {pnl_bar_draw, NULL, pnl_one_row, NULL, pnl_bar_release};
    struct pnl_bar_widget *widget;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    widget = pnl_mem_alloc(&app->mem, sizeof(*widget));
    if (widget == NULL)
        return PNL_ERR_NOMEM;
    memset(widget, 0, sizeof(*widget));
    widget->app = app;
    widget->flags = flags;
    if (pnl_widget_copy(app, &widget->label, label) != PNL_OK) {
        pnl_bar_release(widget);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_BAR, &bar_class, widget, out_widget);
}

static void pnl_graph_bounds(const struct pnl_graph_widget *widget, double *min, double *max)
{
    size_t i;

    *min = widget->min;
    *max = widget->max;
    if (widget->max > widget->min)
        return;
    *min = widget->len > 0 ? widget->values[0] : 0.0;
    *max = *min;
    for (i = 1; i < widget->len; i++) {
        *min = widget->values[i] < *min ? widget->values[i] : *min;
        *max = widget->values[i] > *max ? widget->values[i] : *max;
    }
    if (widget->kind != PNL_GRAPH_PLOT || *max <= *min)
        *min = *min < 0.0 ? *min : 0.0;
    if (*max <= *min)
        *max = *min + 1.0;
}

static void pnl_graph_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_graph_widget *widget = ctx;
    int width = pnl_canvas_width(canvas);
    int top = 0;
    int rows;
    double min;
    double max;

    (void)app;
    if (widget->label != NULL) {
        pnl_canvas_text(canvas, 0, 0, PNL_FG(PNL_ROLE_MUTED), widget->label);
        top = 1;
    }
    rows = widget->rows > 0 ? widget->rows : pnl_canvas_height(canvas) - top;
    if (rows > pnl_canvas_height(canvas) - top)
        rows = pnl_canvas_height(canvas) - top;
    if (rows <= 0)
        return;
    pnl_graph_bounds(widget, &min, &max);
    if (widget->kind == PNL_GRAPH_SPARKLINE)
        pnl_canvas_sparkline(canvas, 0, top, width, widget->values, widget->len, max, widget->flags);
    else if (widget->kind == PNL_GRAPH_CHART)
        pnl_canvas_chart(canvas, 0, top, width, rows, widget->values, widget->len, max, widget->flags);
    else
        pnl_canvas_plot(canvas, 0, top, width, rows, widget->values, widget->len, min, max, widget->flags);
}

static int pnl_graph_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_graph_widget *widget = ctx;

    (void)app;
    (void)width;
    if (widget->rows == 0)
        return 0;
    return widget->rows + (widget->label != NULL);
}

static void pnl_graph_release(void *ctx)
{
    struct pnl_graph_widget *widget = ctx;

    pnl_mem_free(&widget->app->mem, widget->values);
    pnl_mem_free(&widget->app->mem, widget->label);
    pnl_mem_free(&widget->app->mem, widget);
}

int pnl_widget_graph(pnl_app_t *app, const char *label, int kind, int rows, int flags, size_t capacity,
                     pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t graph_class = {pnl_graph_draw, NULL, pnl_graph_height, NULL,
                                                   pnl_graph_release};
    struct pnl_graph_widget *widget;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || kind < PNL_GRAPH_SPARKLINE || kind > PNL_GRAPH_PLOT || rows < 0 ||
        rows > PNL_PLOT_ROWS || capacity == 0 || capacity > PNL_GRAPH_CAPACITY_MAX)
        return PNL_ERR_ARG;
    widget = pnl_mem_alloc(&app->mem, sizeof(*widget));
    if (widget == NULL)
        return PNL_ERR_NOMEM;
    memset(widget, 0, sizeof(*widget));
    widget->app = app;
    widget->kind = kind;
    widget->rows = kind == PNL_GRAPH_SPARKLINE ? 1 : rows;
    widget->flags = flags;
    widget->capacity = capacity;
    widget->values = pnl_mem_alloc(&app->mem, capacity * sizeof(*widget->values));
    if (widget->values == NULL || pnl_widget_copy(app, &widget->label, label) != PNL_OK) {
        pnl_graph_release(widget);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_GRAPH, &graph_class, widget, out_widget);
}

static void pnl_spinner_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_spinner_widget *widget = ctx;

    (void)app;
    pnl_canvas_spinner(canvas, 0, 0, PNL_FG(PNL_ROLE_ACCENT), widget->kind);
    if (widget->text != NULL)
        pnl_canvas_text(canvas, 2, 0, 0, widget->text);
}

static void pnl_spinner_release(void *ctx)
{
    struct pnl_spinner_widget *widget = ctx;

    pnl_mem_free(&widget->app->mem, widget->text);
    pnl_mem_free(&widget->app->mem, widget);
}

int pnl_widget_spinner(pnl_app_t *app, int kind, const char *text, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t spinner_class = {pnl_spinner_draw, NULL, pnl_one_row, NULL,
                                                     pnl_spinner_release};
    struct pnl_spinner_widget *widget;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || kind < PNL_SPINNER_DOTS || kind > PNL_SPINNER_BOUNCE)
        return PNL_ERR_ARG;
    widget = pnl_mem_alloc(&app->mem, sizeof(*widget));
    if (widget == NULL)
        return PNL_ERR_NOMEM;
    memset(widget, 0, sizeof(*widget));
    widget->app = app;
    widget->kind = kind;
    if (pnl_widget_copy(app, &widget->text, text) != PNL_OK) {
        pnl_spinner_release(widget);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_SPINNER, &spinner_class, widget, out_widget);
}

int pnl_widget_set_text(pnl_widget_t *widget, const char *text)
{
    if (widget == NULL || text == NULL)
        return PNL_ERR_ARG;
    if (widget->kind == PNL_WIDGET_TEXT)
        return pnl_widget_copy(widget->app, &((struct pnl_text_widget *)widget->ctx)->text, text);
    if (widget->kind == PNL_WIDGET_SPINNER)
        return pnl_widget_copy(widget->app, &((struct pnl_spinner_widget *)widget->ctx)->text, text);
    if (widget->kind == PNL_WIDGET_BAR)
        return pnl_widget_copy(widget->app, &((struct pnl_bar_widget *)widget->ctx)->label, text);
    if (widget->kind == PNL_WIDGET_GRAPH)
        return pnl_widget_copy(widget->app, &((struct pnl_graph_widget *)widget->ctx)->label, text);
    return pnl_form_set_text(widget, text);
}

static void pnl_graph_push(struct pnl_graph_widget *widget, double value)
{
    if (widget->len == widget->capacity) {
        memmove(widget->values, widget->values + 1, (widget->capacity - 1) * sizeof(*widget->values));
        widget->len--;
    }
    widget->values[widget->len++] = value;
}

int pnl_widget_set_value(pnl_widget_t *widget, double value)
{
    if (widget == NULL || !(value == value))
        return PNL_ERR_ARG;
    if (widget->kind == PNL_WIDGET_BAR) {
        struct pnl_bar_widget *bar = widget->ctx;

        pnl_glide_to(widget->app, &bar->glide, value, PNL_MOTION_COUNT);
    } else if (widget->kind == PNL_WIDGET_GRAPH) {
        pnl_graph_push(widget->ctx, value);
    } else {
        return pnl_form_set_value(widget, value);
    }
    widget->app->dirty = 1;
    return PNL_OK;
}

int pnl_widget_set_range(pnl_widget_t *widget, double min, double max)
{
    struct pnl_graph_widget *graph;

    if (widget != NULL && widget->kind == PNL_WIDGET_SLIDER)
        return pnl_form_set_range(widget, min, max);
    if (widget == NULL || widget->kind != PNL_WIDGET_GRAPH || !(max >= min))
        return PNL_ERR_ARG;
    graph = widget->ctx;
    graph->min = min;
    graph->max = max;
    widget->app->dirty = 1;
    return PNL_OK;
}

static int pnl_stack_sizes(struct pnl_stack_widget *stack, int length, int across)
{
    int left = length - (stack->kind == PNL_STACK_COLUMNS && stack->len > 0 ? (int)stack->len - 1 : 0);
    int flexible = 0;
    size_t i;

    for (i = 0; i < stack->len; i++) {
        struct pnl_stack_child *child = &stack->children[i];

        child->height = child->size;
        if (child->size == 0 && stack->kind == PNL_STACK_ROWS)
            child->height = pnl_widget_height(child->widget, across);
        if (child->height > 0)
            left -= child->height;
        else
            flexible++;
    }
    return flexible > 0 && left > 0 ? left / flexible : 0;
}

static int pnl_stack_first(const struct pnl_stack_widget *stack, int from, int step)
{
    int i;

    for (i = from; i >= 0 && i < (int)stack->len; i += step) {
        if (pnl_widget_focusable(stack->children[i].widget))
            return i;
    }
    return -1;
}

static int pnl_stack_current(struct pnl_stack_widget *stack)
{
    if (stack->focus < 0 || stack->focus >= (int)stack->len ||
        !pnl_widget_focusable(stack->children[stack->focus].widget))
        stack->focus = pnl_stack_first(stack, 0, 1);
    return stack->focus;
}

static int pnl_stack_child_focused(struct pnl_stack_widget *stack, const pnl_canvas_t *canvas, size_t i)
{
    if (!pnl_widget_focusable(stack->children[i].widget))
        return canvas->focused;
    return canvas->focused && (int)i == pnl_stack_current(stack);
}

static void pnl_stack_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_stack_widget *stack = ctx;
    int columns = stack->kind == PNL_STACK_COLUMNS;
    int length = columns ? pnl_canvas_width(canvas) : pnl_canvas_height(canvas);
    int across = columns ? pnl_canvas_height(canvas) : pnl_canvas_width(canvas);
    int share = pnl_stack_sizes(stack, length, pnl_canvas_width(canvas));
    int pos = 0;
    size_t i;

    (void)app;
    for (i = 0; i < stack->len; i++) {
        struct pnl_stack_child *child = &stack->children[i];
        int size = child->height > 0 ? child->height : share;

        size = size > length - pos ? length - pos : size;
        child->x = columns ? pos : 0;
        child->y = columns ? 0 : pos;
        child->width = columns ? size : across;
        child->height = columns ? across : size;
        if (size > 0)
            pnl_widget_place(child->widget, canvas, child->x, child->y, child->width, child->height,
                             pnl_stack_child_focused(stack, canvas, i));
        child->x += canvas->x;
        child->y += canvas->y;
        pos += size + columns;
    }
}

static int pnl_stack_mouse(struct pnl_stack_widget *stack, pnl_app_t *app, int key)
{
    size_t i;

    for (i = 0; i < stack->len; i++) {
        const struct pnl_stack_child *child = &stack->children[i];
        int x = app->term.mouse_x;
        int y = app->term.mouse_y;

        if (x < child->x || y < child->y || x >= child->x + child->width || y >= child->y + child->height)
            continue;
        app->mouse_ox = child->x;
        app->mouse_oy = child->y;
        if (key == PNL_KEY_CLICK && pnl_widget_focusable(child->widget)) {
            stack->focus = (int)i;
            app->dirty = 1;
        }
        return pnl_widget_key(child->widget, key) || (key == PNL_KEY_CLICK && stack->focus == (int)i);
    }
    return 0;
}

static int pnl_stack_step(struct pnl_stack_widget *stack, int key)
{
    int back = key == PNL_KEY_BACKTAB || key == PNL_KEY_UP || key == PNL_KEY_LEFT;
    int rows = stack->kind == PNL_STACK_ROWS && (key == PNL_KEY_UP || key == PNL_KEY_DOWN);
    int columns = stack->kind == PNL_STACK_COLUMNS && (key == PNL_KEY_LEFT || key == PNL_KEY_RIGHT);
    int next;

    if (key != PNL_KEY_TAB && key != PNL_KEY_BACKTAB && key != PNL_KEY_ENTER && !rows && !columns)
        return 0;
    next = pnl_stack_first(stack, stack->focus + (back ? -1 : 1), back ? -1 : 1);
    if (next < 0)
        return 0;
    stack->focus = next;
    pnl_widget_focus_edge(stack->children[next].widget, back);
    stack->app->dirty = 1;
    return 1;
}

static int pnl_stack_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_stack_widget *stack = ctx;
    int focus;
    size_t i;

    if (pnl_is_mouse_key(key))
        return pnl_stack_mouse(stack, app, key);
    focus = pnl_stack_current(stack);
    if (focus >= 0 && pnl_widget_key(stack->children[focus].widget, key))
        return 1;
    if (focus >= 0 && pnl_stack_step(stack, key))
        return 1;
    for (i = 0; i < stack->len; i++) {
        if (!pnl_widget_focusable(stack->children[i].widget) && pnl_widget_key(stack->children[i].widget, key))
            return 1;
    }
    return 0;
}

static int pnl_stack_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_stack_widget *stack = ctx;
    int total = 0;
    size_t i;

    (void)app;
    for (i = 0; i < stack->len; i++) {
        int size = stack->children[i].size > 0 ? stack->children[i].size
                                                : pnl_widget_height(stack->children[i].widget, width);

        if (size == 0)
            return 0;
        total = stack->kind == PNL_STACK_ROWS ? total + size : (size > total ? size : total);
    }
    return total;
}

static const char *pnl_stack_hints(void *ctx)
{
    struct pnl_stack_widget *stack = ctx;
    int focus = pnl_stack_current(stack);
    size_t i;

    if (focus >= 0 && pnl_widget_hints(stack->children[focus].widget) != NULL)
        return pnl_widget_hints(stack->children[focus].widget);
    for (i = 0; i < stack->len; i++) {
        const char *hints = pnl_widget_hints(stack->children[i].widget);

        if (hints != NULL)
            return hints;
    }
    return NULL;
}

static void pnl_stack_release(void *ctx)
{
    struct pnl_stack_widget *stack = ctx;

    pnl_mem_free(&stack->app->mem, stack->children);
    pnl_mem_free(&stack->app->mem, stack);
}

int pnl_widget_stack(pnl_app_t *app, int kind, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t stack_class = {pnl_stack_draw, pnl_stack_key, pnl_stack_height,
                                                   pnl_stack_hints, pnl_stack_release};
    struct pnl_stack_widget *stack;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || (kind != PNL_STACK_ROWS && kind != PNL_STACK_COLUMNS))
        return PNL_ERR_ARG;
    stack = pnl_mem_alloc(&app->mem, sizeof(*stack));
    if (stack == NULL)
        return PNL_ERR_NOMEM;
    memset(stack, 0, sizeof(*stack));
    stack->app = app;
    stack->kind = kind;
    stack->focus = -1;
    return pnl_widget_builtin(app, PNL_WIDGET_STACK, &stack_class, stack, out_widget);
}

int pnl_widget_add(pnl_widget_t *stack, pnl_widget_t *child, int size)
{
    struct pnl_stack_widget *ctx;
    int status;

    if (stack == NULL || child == NULL || stack == child || stack->kind != PNL_WIDGET_STACK ||
        child->app != stack->app || size < 0)
        return PNL_ERR_ARG;
    ctx = stack->ctx;
    status = pnl_mem_grow(&stack->app->mem, (void **)&ctx->children, &ctx->cap, ctx->len + 1,
                          sizeof(*ctx->children));
    if (status != PNL_OK)
        return status;
    memset(&ctx->children[ctx->len], 0, sizeof(ctx->children[0]));
    ctx->children[ctx->len].widget = child;
    ctx->children[ctx->len].size = size;
    ctx->len++;
    stack->app->dirty = 1;
    return PNL_OK;
}

int pnl_widget_focusable(const pnl_widget_t *widget)
{
    const struct pnl_stack_widget *stack;
    size_t i;

    if (widget == NULL)
        return 0;
    if (widget->kind != PNL_WIDGET_STACK)
        return widget->focusable;
    stack = widget->ctx;
    for (i = 0; i < stack->len; i++) {
        if (pnl_widget_focusable(stack->children[i].widget))
            return 1;
    }
    return 0;
}

void pnl_widget_focus_edge(pnl_widget_t *widget, int last)
{
    struct pnl_stack_widget *stack;

    if (widget == NULL || widget->kind != PNL_WIDGET_STACK)
        return;
    stack = widget->ctx;
    stack->focus = last ? pnl_stack_first(stack, (int)stack->len - 1, -1) : pnl_stack_first(stack, 0, 1);
    if (stack->focus >= 0)
        pnl_widget_focus_edge(stack->children[stack->focus].widget, last);
    widget->app->dirty = 1;
}

void pnl_widget_changed(pnl_widget_t *widget)
{
    widget->app->dirty = 1;
    if (widget->change_fn != NULL)
        widget->change_fn(widget->change_ctx, widget->app, widget);
}

int pnl_widget_on_change(pnl_widget_t *widget, pnl_change_fn change_fn, void *change_ctx)
{
    if (widget == NULL)
        return PNL_ERR_ARG;
    widget->change_fn = change_fn;
    widget->change_ctx = change_ctx;
    return PNL_OK;
}

int pnl_widget_set_focusable(pnl_widget_t *widget, int focusable)
{
    if (widget == NULL || widget->kind == PNL_WIDGET_STACK)
        return PNL_ERR_ARG;
    widget->focusable = focusable != 0;
    widget->app->dirty = 1;
    return PNL_OK;
}

int pnl_widget_focus(pnl_widget_t *stack, pnl_widget_t *child)
{
    struct pnl_stack_widget *ctx;
    size_t i;

    if (stack == NULL || child == NULL || stack->kind != PNL_WIDGET_STACK || !pnl_widget_focusable(child))
        return PNL_ERR_ARG;
    ctx = stack->ctx;
    for (i = 0; i < ctx->len; i++) {
        pnl_widget_t *inner = ctx->children[i].widget;

        if (inner == child || (inner->kind == PNL_WIDGET_STACK && pnl_widget_focus(inner, child) == PNL_OK)) {
            ctx->focus = (int)i;
            stack->app->dirty = 1;
            return PNL_OK;
        }
    }
    return PNL_ERR_NOT_FOUND;
}

const char *pnl_widget_get_text(const pnl_widget_t *widget)
{
    if (widget == NULL)
        return NULL;
    if (widget->kind == PNL_WIDGET_TEXT)
        return ((const struct pnl_text_widget *)widget->ctx)->text;
    if (widget->kind == PNL_WIDGET_SPINNER)
        return ((const struct pnl_spinner_widget *)widget->ctx)->text;
    if (widget->kind == PNL_WIDGET_BAR)
        return ((const struct pnl_bar_widget *)widget->ctx)->label;
    if (widget->kind == PNL_WIDGET_GRAPH)
        return ((const struct pnl_graph_widget *)widget->ctx)->label;
    return pnl_form_get_text(widget);
}

int pnl_widget_get_value(const pnl_widget_t *widget, double *out_value)
{
    const struct pnl_graph_widget *graph;

    if (widget == NULL || out_value == NULL)
        return PNL_ERR_ARG;
    if (widget->kind == PNL_WIDGET_BAR) {
        *out_value = ((const struct pnl_bar_widget *)widget->ctx)->glide.to;
        return PNL_OK;
    }
    if (widget->kind != PNL_WIDGET_GRAPH)
        return pnl_form_get_value(widget, out_value);
    graph = widget->ctx;
    if (graph->len == 0)
        return PNL_ERR_NOT_FOUND;
    *out_value = graph->values[graph->len - 1];
    return PNL_OK;
}
