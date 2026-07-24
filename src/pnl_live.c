#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_STEPS_MAX 16
#define PNL_STEP_LABEL 32
#define PNL_STEP_DETAIL 48
#define PNL_SCENES_MAX 16
#define PNL_FRAMES_MAX 16
#define PNL_ARROW 3

struct pnl_step {
    char label[PNL_STEP_LABEL];
    char detail[PNL_STEP_DETAIL];
    int state;
};

struct pnl_flow_widget {
    pnl_app_t *app;
    struct pnl_lock lock;
    struct pnl_step steps[PNL_STEPS_MAX];
    int len;
};

struct pnl_scene {
    int state;
    char *frames[PNL_FRAMES_MAX];
    int frames_len;
    int style;
    int gradient;
    int frame_ms;
};

struct pnl_scene_widget {
    pnl_app_t *app;
    struct pnl_lock lock;
    struct pnl_scene scenes[PNL_SCENES_MAX];
    int len;
    int state;
};

static const int pnl_step_roles[] = {PNL_ROLE_MUTED, PNL_ROLE_ACCENT, PNL_ROLE_OK, PNL_ROLE_FAIL, PNL_ROLE_MUTED};
static const char *const pnl_step_icons[] = {"\xc2\xb7", "", "\xe2\x9c\x93", "\xe2\x9c\x97", "-"};

static void pnl_live_copy(char *dest, size_t cap, const char *text)
{
    size_t len = strlen(text);

    if (len >= cap) {
        len = cap - 1;
        while (len > 0 && ((unsigned char)text[len] & 0xc0) == 0x80)
            len--;
    }
    memcpy(dest, text, len);
    dest[len] = '\0';
}

static int pnl_box_width(const struct pnl_step *step)
{
    return pnl_text_width(step->label) + 6;
}

static int pnl_flow_wide(const struct pnl_flow_widget *flow)
{
    int width = 0;
    int i;

    for (i = 0; i < flow->len; i++)
        width += pnl_box_width(&flow->steps[i]) + (i > 0 ? PNL_ARROW : 0);
    return width;
}

static int pnl_flow_details(const struct pnl_flow_widget *flow)
{
    int i;

    for (i = 0; i < flow->len; i++) {
        if (flow->steps[i].detail[0] != '\0')
            return 1;
    }
    return 0;
}

static void pnl_step_icon(pnl_canvas_t *canvas, int x, int y, const struct pnl_step *step)
{
    int style = PNL_BOLD | PNL_FG(pnl_step_roles[step->state]);

    if (step->state == PNL_STEP_RUN)
        pnl_canvas_spinner(canvas, x, y, style, PNL_SPINNER_DOTS);
    else
        pnl_canvas_text(canvas, x, y, style, pnl_step_icons[step->state]);
}

static void pnl_flow_line(pnl_canvas_t *canvas, int x, int y, int width, const char *left, const char *right,
                          int style)
{
    int i;

    pnl_canvas_text(canvas, x, y, style, left);
    for (i = 1; i < width - 1; i++)
        pnl_canvas_text(canvas, x + i, y, style, "\xe2\x94\x80");
    pnl_canvas_text(canvas, x + width - 1, y, style, right);
}

static void pnl_flow_box(pnl_canvas_t *canvas, int x, const struct pnl_step *step, int detail_width)
{
    int width = pnl_box_width(step);
    int border = (step->state == PNL_STEP_RUN ? PNL_BOLD : 0) | PNL_FG(pnl_step_roles[step->state]);
    int label = step->state == PNL_STEP_WAIT || step->state == PNL_STEP_SKIP ? PNL_FG(PNL_ROLE_MUTED) : PNL_BOLD;

    pnl_flow_line(canvas, x, 0, width, "\xe2\x94\x8c", "\xe2\x94\x90", border);
    pnl_canvas_text(canvas, x, 1, border, "\xe2\x94\x82");
    pnl_step_icon(canvas, x + 2, 1, step);
    pnl_canvas_text(canvas, x + 4, 1, label, step->label);
    pnl_canvas_text(canvas, x + width - 1, 1, border, "\xe2\x94\x82");
    pnl_flow_line(canvas, x, 2, width, "\xe2\x94\x94", "\xe2\x94\x98", border);
    if (detail_width > 0)
        pnl_canvas_clip(canvas, x + 1, 3, detail_width, PNL_FG(PNL_ROLE_MUTED), step->detail);
}

static void pnl_flow_arrow(pnl_canvas_t *canvas, int x, int y, const struct pnl_step *from, const struct pnl_step *to)
{
    int spin = canvas->screen->spin;
    int role = from->state == PNL_STEP_OK ? PNL_ROLE_OK : PNL_ROLE_MUTED;
    int i;

    if (to->state == PNL_STEP_RUN)
        role = PNL_ROLE_ACCENT;
    for (i = 0; i < PNL_ARROW; i++) {
        int lit = to->state == PNL_STEP_RUN && spin >= 0 && spin / 3 % PNL_ARROW == i;
        const char *glyph = i == PNL_ARROW - 1 ? ">" : "\xe2\x94\x80";

        pnl_canvas_text(canvas, x + i, y, lit ? PNL_BOLD | PNL_REVERSE | PNL_FG(role) : PNL_FG(role), glyph);
    }
    if (to->state == PNL_STEP_RUN && spin >= 0)
        canvas->screen->spinning = 1;
}

static void pnl_flow_across(const struct pnl_flow_widget *flow, pnl_canvas_t *canvas)
{
    int details = pnl_flow_details(flow);
    int room;
    int x = 0;
    int i;

    for (i = 0; i < flow->len; i++) {
        if (i > 0) {
            pnl_flow_arrow(canvas, x, 1, &flow->steps[i - 1], &flow->steps[i]);
            x += PNL_ARROW;
        }
        room = i + 1 < flow->len ? pnl_box_width(&flow->steps[i]) + PNL_ARROW - 1 : pnl_canvas_width(canvas) - x - 1;
        pnl_flow_box(canvas, x, &flow->steps[i], details ? room : 0);
        x += pnl_box_width(&flow->steps[i]);
    }
}

static void pnl_flow_down(const struct pnl_flow_widget *flow, pnl_canvas_t *canvas)
{
    int label_width = 0;
    int i;

    for (i = 0; i < flow->len; i++) {
        int w = pnl_text_width(flow->steps[i].label);

        label_width = w > label_width ? w : label_width;
    }
    for (i = 0; i < flow->len; i++) {
        const struct pnl_step *step = &flow->steps[i];
        int y = i * 2;

        if (i > 0) {
            int lit = step->state == PNL_STEP_RUN;
            int role = lit ? PNL_ROLE_ACCENT : flow->steps[i - 1].state == PNL_STEP_OK ? PNL_ROLE_OK : PNL_ROLE_MUTED;

            pnl_canvas_text(canvas, 0, y - 1, PNL_FG(role), "\xe2\x94\x82");
        }
        pnl_step_icon(canvas, 0, y, step);
        pnl_canvas_text(canvas, 2, y, step->state == PNL_STEP_WAIT ? PNL_FG(PNL_ROLE_MUTED) : PNL_BOLD, step->label);
        pnl_canvas_text(canvas, label_width + 4, y, PNL_FG(PNL_ROLE_MUTED), step->detail);
    }
}

static void pnl_flow_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_flow_widget *flow = ctx;

    (void)app;
    pnl_lock(&flow->lock);
    if (pnl_flow_wide(flow) <= pnl_canvas_width(canvas) && !canvas->screen->linear)
        pnl_flow_across(flow, canvas);
    else
        pnl_flow_down(flow, canvas);
    pnl_unlock(&flow->lock);
}

static int pnl_flow_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_flow_widget *flow = ctx;
    int height;

    pnl_lock(&flow->lock);
    if (pnl_flow_wide(flow) <= width && !app->screen.linear)
        height = 3 + pnl_flow_details(flow);
    else
        height = flow->len > 0 ? flow->len * 2 - 1 : 1;
    pnl_unlock(&flow->lock);
    return height;
}

static void pnl_flow_release(void *ctx)
{
    struct pnl_flow_widget *flow = ctx;

    pnl_lock_destroy(&flow->lock);
    pnl_mem_free(&flow->app->mem, flow);
}

int pnl_widget_flow(pnl_app_t *app, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t flow_class = {pnl_flow_draw, NULL, pnl_flow_height, NULL, pnl_flow_release};
    struct pnl_flow_widget *flow;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    flow = pnl_mem_alloc(&app->mem, sizeof(*flow));
    if (flow == NULL)
        return PNL_ERR_NOMEM;
    memset(flow, 0, sizeof(*flow));
    flow->app = app;
    if (pnl_lock_init(&flow->lock) != PNL_OK) {
        pnl_mem_free(&app->mem, flow);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_FLOW, &flow_class, flow, out_widget);
}

int pnl_widget_add_step(pnl_widget_t *widget, const char *label, int *out_step)
{
    struct pnl_flow_widget *flow;
    int status = PNL_OK;

    if (out_step != NULL)
        *out_step = -1;
    if (widget == NULL || label == NULL || widget->kind != PNL_WIDGET_FLOW)
        return PNL_ERR_ARG;
    flow = widget->ctx;
    pnl_lock(&flow->lock);
    if (flow->len == PNL_STEPS_MAX) {
        status = PNL_ERR_NOMEM;
    } else {
        memset(&flow->steps[flow->len], 0, sizeof(flow->steps[0]));
        pnl_live_copy(flow->steps[flow->len].label, PNL_STEP_LABEL, label);
        if (out_step != NULL)
            *out_step = flow->len;
        flow->len++;
    }
    pnl_unlock(&flow->lock);
    widget->app->dirty = 1;
    return status;
}

int pnl_widget_set_step(pnl_widget_t *widget, int step, int state, const char *detail)
{
    struct pnl_flow_widget *flow;
    int status = PNL_OK;

    if (widget == NULL || widget->kind != PNL_WIDGET_FLOW || state < PNL_STEP_WAIT || state > PNL_STEP_SKIP)
        return PNL_ERR_ARG;
    flow = widget->ctx;
    pnl_lock(&flow->lock);
    if (step < 0 || step >= flow->len) {
        status = PNL_ERR_NOT_FOUND;
    } else {
        flow->steps[step].state = state;
        pnl_live_copy(flow->steps[step].detail, PNL_STEP_DETAIL, detail != NULL ? detail : "");
    }
    pnl_unlock(&flow->lock);
    if (status == PNL_OK)
        pnl_term_wake(&widget->app->term);
    return status;
}

static int pnl_scene_find(const struct pnl_scene_widget *scene, int state)
{
    int i;

    for (i = 0; i < scene->len; i++) {
        if (scene->scenes[i].state == state)
            return i;
    }
    return -1;
}

static const struct pnl_scene *pnl_scene_current(const struct pnl_scene_widget *scene)
{
    int slot = pnl_scene_find(scene, scene->state);

    return slot >= 0 ? &scene->scenes[slot] : NULL;
}

static void pnl_scene_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_scene_widget *scene = ctx;
    const struct pnl_scene *shown;
    int frame = 0;

    if (canvas->screen->linear)
        return;
    pnl_lock(&scene->lock);
    shown = pnl_scene_current(scene);
    if (shown != NULL && shown->frames_len > 1 && canvas->screen->spin >= 0) {
        frame = (int)(pnl_app_frame_ms(app) / shown->frame_ms % shown->frames_len);
        canvas->screen->spinning = 1;
    }
    if (shown != NULL)
        pnl_canvas_art(canvas, 0, 0, shown->style, shown->gradient, shown->frames[frame]);
    pnl_unlock(&scene->lock);
}

static int pnl_scene_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_scene_widget *scene = ctx;
    int height = 0;
    int i;
    int j;

    (void)width;
    if (app->screen.linear)
        return 0;
    pnl_lock(&scene->lock);
    for (i = 0; i < scene->len; i++) {
        for (j = 0; j < scene->scenes[i].frames_len; j++) {
            int art_width = 0;
            int h = pnl_art_size(scene->scenes[i].frames[j], &art_width);

            height = h > height ? h : height;
        }
    }
    pnl_unlock(&scene->lock);
    return height;
}

static void pnl_scene_free(struct pnl_scene_widget *scene, struct pnl_scene *shown)
{
    int i;

    for (i = 0; i < shown->frames_len; i++)
        pnl_mem_free(&scene->app->mem, shown->frames[i]);
    shown->frames_len = 0;
}

static void pnl_scene_release(void *ctx)
{
    struct pnl_scene_widget *scene = ctx;
    int i;

    for (i = 0; i < scene->len; i++)
        pnl_scene_free(scene, &scene->scenes[i]);
    pnl_lock_destroy(&scene->lock);
    pnl_mem_free(&scene->app->mem, scene);
}

int pnl_widget_scene(pnl_app_t *app, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t scene_class = {pnl_scene_draw, NULL, pnl_scene_height, NULL, pnl_scene_release};
    struct pnl_scene_widget *scene;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    scene = pnl_mem_alloc(&app->mem, sizeof(*scene));
    if (scene == NULL)
        return PNL_ERR_NOMEM;
    memset(scene, 0, sizeof(*scene));
    scene->app = app;
    if (pnl_lock_init(&scene->lock) != PNL_OK) {
        pnl_mem_free(&app->mem, scene);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, PNL_WIDGET_SCENE, &scene_class, scene, out_widget);
}

static int pnl_scene_split(struct pnl_scene_widget *scene, struct pnl_scene *shown, const char *frames)
{
    const char *start = frames;

    while (start != NULL && shown->frames_len < PNL_FRAMES_MAX) {
        const char *end = (strchr)(start, '\f');
        size_t len = end != NULL ? (size_t)(end - start) : strlen(start);
        char *copy = pnl_mem_alloc(&scene->app->mem, len + 1);

        if (copy == NULL)
            return PNL_ERR_NOMEM;
        memcpy(copy, start, len);
        copy[len] = '\0';
        shown->frames[shown->frames_len++] = copy;
        start = end != NULL ? end + 1 : NULL;
    }
    return PNL_OK;
}

int pnl_widget_add_scene(pnl_widget_t *widget, int state, const char *frames, int style, int gradient, int frame_ms)
{
    struct pnl_scene_widget *scene;
    struct pnl_scene next;
    struct pnl_scene old;
    int slot;
    int status;

    if (widget == NULL || frames == NULL || widget->kind != PNL_WIDGET_SCENE || frame_ms < 0)
        return PNL_ERR_ARG;
    scene = widget->ctx;
    memset(&next, 0, sizeof(next));
    next.state = state;
    next.style = style;
    next.gradient = gradient;
    next.frame_ms = frame_ms > 0 ? frame_ms : 500;
    status = pnl_scene_split(scene, &next, frames);
    pnl_lock(&scene->lock);
    slot = pnl_scene_find(scene, state);
    if (status == PNL_OK && slot < 0 && scene->len == PNL_SCENES_MAX)
        status = PNL_ERR_NOMEM;
    if (status == PNL_OK && slot >= 0) {
        old = scene->scenes[slot];
        scene->scenes[slot] = next;
        next = old;
    } else if (status == PNL_OK) {
        scene->scenes[scene->len++] = next;
        next.frames_len = 0;
    }
    pnl_unlock(&scene->lock);
    pnl_scene_free(scene, &next);
    widget->app->dirty = 1;
    return status;
}

int pnl_scene_set_state(pnl_widget_t *widget, double value)
{
    struct pnl_scene_widget *scene = widget->ctx;
    int state = (int)value;
    int changed;

    pnl_lock(&scene->lock);
    changed = scene->state != state;
    scene->state = state;
    pnl_unlock(&scene->lock);
    if (changed)
        pnl_term_wake(&widget->app->term);
    return PNL_OK;
}

int pnl_scene_state(const pnl_widget_t *widget, double *out_value)
{
    struct pnl_scene_widget *scene = widget->ctx;

    pnl_lock(&scene->lock);
    *out_value = scene->state;
    pnl_unlock(&scene->lock);
    return PNL_OK;
}
