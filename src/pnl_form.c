#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_FIELD_MAX 256
#define PNL_PRESS_MS 250

struct pnl_input_widget {
    pnl_app_t *app;
    pnl_widget_t *self;
    char *label;
    int flags;
    char text[PNL_FIELD_MAX];
    struct pnl_edit edit;
};

struct pnl_check_widget {
    pnl_app_t *app;
    pnl_widget_t *self;
    char *label;
    int checked;
};

struct pnl_choice_widget {
    pnl_app_t *app;
    pnl_widget_t *self;
    char *label;
    char **options;
    size_t count;
    size_t index;
};

struct pnl_slider_widget {
    pnl_app_t *app;
    pnl_widget_t *self;
    char *label;
    double min;
    double max;
    double step;
    double value;
    struct pnl_glide glide;
    int bar_x;
    int bar_width;
};

struct pnl_button_widget {
    pnl_app_t *app;
    pnl_widget_t *self;
    char *label;
    pnl_action_fn action_fn;
    void *action_ctx;
    long long pressed_ms;
};

static int pnl_form_label(pnl_canvas_t *canvas, const char *label)
{
    int style = pnl_canvas_focused(canvas) ? PNL_BOLD | PNL_FG(PNL_ROLE_ACCENT) : PNL_FG(PNL_ROLE_MUTED);

    if (label == NULL)
        return 0;
    pnl_canvas_text(canvas, 0, 0, style, label);
    return pnl_text_width(label) + 1;
}

static int pnl_form_mouse_x(const pnl_app_t *app)
{
    return app->term.mouse_x - app->mouse_ox;
}

static void pnl_form_release(pnl_app_t *app, char *label, void *widget)
{
    pnl_mem_free(&app->mem, label);
    pnl_mem_free(&app->mem, widget);
}

static int pnl_field_line(void *ctx, pnl_app_t *app, int width)
{
    (void)ctx;
    (void)app;
    (void)width;
    return 1;
}

static void *pnl_form_alloc(pnl_app_t *app, size_t size)
{
    void *widget = pnl_mem_alloc(&app->mem, size);

    if (widget != NULL)
        memset(widget, 0, size);
    return widget;
}

static int pnl_input_set(pnl_widget_t *widget, const char *text)
{
    struct pnl_input_widget *input = widget->ctx;
    size_t len = strlen(text);

    if (len >= sizeof(input->text))
        return PNL_ERR_ARG;
    memcpy(input->text, text, len + 1);
    pnl_edit_init(&input->edit, input->text, sizeof(input->text));
    widget->app->dirty = 1;
    return PNL_OK;
}

static const char *pnl_input_get(const pnl_widget_t *widget)
{
    return ((const struct pnl_input_widget *)widget->ctx)->text;
}

static int pnl_check_set_label(pnl_widget_t *widget, const char *text)
{
    return pnl_widget_copy(widget->app, &((struct pnl_check_widget *)widget->ctx)->label, text);
}

static const char *pnl_check_label(const pnl_widget_t *widget)
{
    return ((const struct pnl_check_widget *)widget->ctx)->label;
}

static int pnl_check_set(pnl_widget_t *widget, double value)
{
    ((struct pnl_check_widget *)widget->ctx)->checked = value != 0.0;
    widget->app->dirty = 1;
    return PNL_OK;
}

static int pnl_check_get(const pnl_widget_t *widget, double *out_value)
{
    *out_value = ((const struct pnl_check_widget *)widget->ctx)->checked;
    return PNL_OK;
}

static int pnl_choice_pick(pnl_widget_t *widget, const char *text)
{
    struct pnl_choice_widget *choice = widget->ctx;
    size_t i;

    for (i = 0; i < choice->count; i++) {
        if (strcmp(choice->options[i], text) == 0) {
            choice->index = i;
            widget->app->dirty = 1;
            return PNL_OK;
        }
    }
    return PNL_ERR_NOT_FOUND;
}

static const char *pnl_choice_option(const pnl_widget_t *widget)
{
    const struct pnl_choice_widget *choice = widget->ctx;

    return choice->options[choice->index];
}

static int pnl_choice_set(pnl_widget_t *widget, double value)
{
    struct pnl_choice_widget *choice = widget->ctx;

    if (value < 0.0 || value >= (double)choice->count)
        return PNL_ERR_ARG;
    choice->index = (size_t)value;
    widget->app->dirty = 1;
    return PNL_OK;
}

static int pnl_choice_get(const pnl_widget_t *widget, double *out_value)
{
    *out_value = (double)((const struct pnl_choice_widget *)widget->ctx)->index;
    return PNL_OK;
}

static int pnl_button_set_label(pnl_widget_t *widget, const char *text)
{
    return pnl_widget_copy(widget->app, &((struct pnl_button_widget *)widget->ctx)->label, text);
}

static const char *pnl_button_label(const pnl_widget_t *widget)
{
    return ((const struct pnl_button_widget *)widget->ctx)->label;
}

static int pnl_form_finish(pnl_app_t *app, int kind, const pnl_widget_class_t *widget_class,
                           const struct pnl_widget_ops *ops, void *ctx, pnl_widget_t **self, pnl_widget_t **out_widget)
{
    int status = pnl_widget_builtin(app, kind, widget_class, ops, ctx, out_widget);

    if (status != PNL_OK)
        return status;
    *self = *out_widget;
    (*out_widget)->focusable = 1;
    return PNL_OK;
}

static void pnl_field_masked(const struct pnl_input_widget *input, char *masked, struct pnl_edit *edit)
{
    size_t i;
    size_t n = 0;
    size_t cursor = 0;

    for (i = 0; i < input->edit.len; i++) {
        if (((unsigned char)input->text[i] & 0xc0) == 0x80)
            continue;
        if (i < input->edit.cursor)
            cursor++;
        masked[n++] = '*';
    }
    masked[n] = '\0';
    edit->text = masked;
    edit->cap = PNL_FIELD_MAX;
    edit->len = n;
    edit->cursor = cursor;
}

static void pnl_field_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_input_widget *input = ctx;
    int focused = pnl_canvas_focused(canvas);
    int x = pnl_form_label(canvas, input->label);
    int width = pnl_canvas_width(canvas) - x;
    char masked[PNL_FIELD_MAX];
    struct pnl_edit shown = input->edit;

    if (width <= 0)
        return;
    if (focused)
        app->widget_typing = 1;
    if (input->flags & PNL_INPUT_SECRET)
        pnl_field_masked(input, masked, &shown);
    pnl_canvas_fill(canvas, x, 0, width, 1, PNL_BG(PNL_ROLE_SURFACE));
    pnl_edit_draw_styled(&shown, canvas, x, 0, width, focused, PNL_BG(PNL_ROLE_SURFACE));
}

static int pnl_field_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_input_widget *input = ctx;
    char before[PNL_FIELD_MAX];

    (void)app;
    if (key == PNL_KEY_CLICK)
        return 1;
    if (pnl_is_mouse_key(key))
        return 0;
    memcpy(before, input->text, input->edit.len + 1);
    if (!pnl_edit_key(&input->edit, key))
        return 0;
    if (strcmp(before, input->text) != 0)
        pnl_widget_changed(input->self);
    return 1;
}

static const char *pnl_field_hints(void *ctx)
{
    (void)ctx;
    return "enter: next field";
}

static void pnl_field_release(void *ctx)
{
    struct pnl_input_widget *input = ctx;

    pnl_form_release(input->app, input->label, input);
}

int pnl_widget_input(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t input_class = {pnl_field_draw, pnl_field_key, pnl_field_line, pnl_field_hints,
                                                   pnl_field_release};
    static const struct pnl_widget_ops input_ops = {pnl_input_set, pnl_input_get, NULL, NULL, NULL};
    struct pnl_input_widget *input;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || (flags & ~PNL_INPUT_SECRET) != 0)
        return PNL_ERR_ARG;
    input = pnl_form_alloc(app, sizeof(*input));
    if (input != NULL)
        input->app = app;
    if (input == NULL || pnl_widget_copy(app, &input->label, label) != PNL_OK) {
        if (input != NULL)
            pnl_field_release(input);
        return PNL_ERR_NOMEM;
    }
    input->app = app;
    input->flags = flags;
    pnl_edit_init(&input->edit, input->text, sizeof(input->text));
    return pnl_form_finish(app, PNL_WIDGET_INPUT, &input_class, &input_ops, input, &input->self, out_widget);
}

static void pnl_check_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_check_widget *check = ctx;
    int focused = pnl_canvas_focused(canvas);
    int box = focused ? PNL_BOLD | PNL_FG(PNL_ROLE_ACCENT) : PNL_FG(PNL_ROLE_MUTED);

    (void)app;
    pnl_canvas_text(canvas, 0, 0, box, check->checked ? "[x]" : "[ ]");
    if (check->label != NULL)
        pnl_canvas_text(canvas, 4, 0, focused ? PNL_BOLD : 0, check->label);
}

static int pnl_check_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_check_widget *check = ctx;

    (void)app;
    if (key != ' ' && key != PNL_KEY_CLICK)
        return 0;
    check->checked = !check->checked;
    pnl_widget_changed(check->self);
    return 1;
}

static const char *pnl_check_hints(void *ctx)
{
    (void)ctx;
    return "space: toggle";
}

static void pnl_check_release(void *ctx)
{
    struct pnl_check_widget *check = ctx;

    pnl_form_release(check->app, check->label, check);
}

int pnl_widget_check(pnl_app_t *app, const char *label, int checked, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t check_class = {pnl_check_draw, pnl_check_key, pnl_field_line, pnl_check_hints,
                                                   pnl_check_release};
    static const struct pnl_widget_ops check_ops = {pnl_check_set_label, pnl_check_label, pnl_check_set, pnl_check_get, NULL};
    struct pnl_check_widget *check;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    check = pnl_form_alloc(app, sizeof(*check));
    if (check != NULL)
        check->app = app;
    if (check == NULL || pnl_widget_copy(app, &check->label, label) != PNL_OK) {
        if (check != NULL)
            pnl_check_release(check);
        return PNL_ERR_NOMEM;
    }
    check->app = app;
    check->checked = checked != 0;
    return pnl_form_finish(app, PNL_WIDGET_CHECK, &check_class, &check_ops, check, &check->self, out_widget);
}

static void pnl_choice_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_choice_widget *choice = ctx;
    int focused = pnl_canvas_focused(canvas);
    int arrows = focused ? PNL_BOLD | PNL_FG(PNL_ROLE_ACCENT) : PNL_FG(PNL_ROLE_MUTED);
    int x = pnl_form_label(canvas, choice->label);

    (void)app;
    x = pnl_canvas_text(canvas, x, 0, arrows, "< ");
    x = pnl_canvas_text(canvas, x, 0, focused ? PNL_BOLD : 0, choice->options[choice->index]);
    pnl_canvas_text(canvas, x, 0, arrows, " >");
}

static int pnl_choice_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_choice_widget *choice = ctx;
    size_t back = choice->index > 0 ? choice->index - 1 : choice->count - 1;

    (void)app;
    if (key == PNL_KEY_LEFT)
        choice->index = back;
    else if (key == PNL_KEY_RIGHT || key == ' ' || key == PNL_KEY_CLICK)
        choice->index = (choice->index + 1) % choice->count;
    else
        return 0;
    pnl_widget_changed(choice->self);
    return 1;
}

static const char *pnl_choice_hints(void *ctx)
{
    (void)ctx;
    return "left/right: change";
}

static void pnl_choice_release(void *ctx)
{
    struct pnl_choice_widget *choice = ctx;
    size_t i;

    for (i = 0; choice->options != NULL && i < choice->count; i++)
        pnl_mem_free(&choice->app->mem, choice->options[i]);
    pnl_mem_free(&choice->app->mem, choice->options);
    pnl_form_release(choice->app, choice->label, choice);
}

static int pnl_choice_copy(struct pnl_choice_widget *choice, const char *const *options, size_t count)
{
    size_t i;

    choice->options = pnl_mem_alloc(&choice->app->mem, count * sizeof(*choice->options));
    if (choice->options == NULL)
        return PNL_ERR_NOMEM;
    memset(choice->options, 0, count * sizeof(*choice->options));
    choice->count = count;
    for (i = 0; i < count; i++) {
        if (pnl_widget_copy(choice->app, &choice->options[i], options[i]) != PNL_OK)
            return PNL_ERR_NOMEM;
    }
    return PNL_OK;
}

int pnl_widget_choice(pnl_app_t *app, const char *label, const char *const *options, size_t count,
                      pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t choice_class = {pnl_choice_draw, pnl_choice_key, pnl_field_line, pnl_choice_hints,
                                                    pnl_choice_release};
    static const struct pnl_widget_ops choice_ops = {pnl_choice_pick, pnl_choice_option, pnl_choice_set, pnl_choice_get, NULL};
    struct pnl_choice_widget *choice;
    size_t i;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || options == NULL || count == 0)
        return PNL_ERR_ARG;
    for (i = 0; i < count; i++) {
        if (options[i] == NULL)
            return PNL_ERR_ARG;
    }
    choice = pnl_form_alloc(app, sizeof(*choice));
    if (choice == NULL)
        return PNL_ERR_NOMEM;
    choice->app = app;
    if (pnl_widget_copy(app, &choice->label, label) != PNL_OK || pnl_choice_copy(choice, options, count) != PNL_OK) {
        pnl_choice_release(choice);
        return PNL_ERR_NOMEM;
    }
    return pnl_form_finish(app, PNL_WIDGET_CHOICE, &choice_class, &choice_ops, choice, &choice->self, out_widget);
}

static double pnl_slider_fit(const struct pnl_slider_widget *slider, double value)
{
    double steps;

    value = value < slider->min ? slider->min : value > slider->max ? slider->max : value;
    if (slider->step <= 0.0)
        return value;
    steps = (value - slider->min) / slider->step + 0.5;
    if (!(steps < 9.0e18))
        return value;
    value = slider->min + (double)(long long)steps * slider->step;
    return value > slider->max ? slider->max : value;
}

static double pnl_slider_share(const struct pnl_slider_widget *slider, double value)
{
    return slider->max > slider->min ? (value - slider->min) / (slider->max - slider->min) : 0.0;
}

static void pnl_slider_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_slider_widget *slider = ctx;
    int focused = pnl_canvas_focused(canvas);
    int x = pnl_form_label(canvas, slider->label);
    char number[32];
    int width;

    snprintf(number, sizeof(number), " %g", slider->value);
    width = pnl_canvas_width(canvas) - x - (int)strlen(number);
    slider->bar_x = x;
    slider->bar_width = width;
    if (width <= 0)
        return;
    pnl_canvas_bar(canvas, x, 0, width, pnl_glide_at(app, &slider->glide, PNL_MOTION_COUNT),
                   focused ? PNL_METER_ACCENT : 0);
    pnl_canvas_text(canvas, x + width, 0, focused ? PNL_BOLD : PNL_FG(PNL_ROLE_MUTED), number);
}

static void pnl_slider_move(struct pnl_slider_widget *slider, double value)
{
    value = pnl_slider_fit(slider, value);
    if (value == slider->value)
        return;
    slider->value = value;
    pnl_glide_to(slider->app, &slider->glide, pnl_slider_share(slider, value), PNL_MOTION_COUNT);
    pnl_widget_changed(slider->self);
}

static int pnl_slider_click(struct pnl_slider_widget *slider, pnl_app_t *app)
{
    int x = pnl_form_mouse_x(app) - slider->bar_x;

    if (x < 0 || x >= slider->bar_width)
        return 1;
    pnl_slider_move(slider, slider->min + (slider->max - slider->min) * (double)(x + 1) / (double)slider->bar_width);
    return 1;
}

static int pnl_slider_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_slider_widget *slider = ctx;
    double step = slider->step > 0.0 ? slider->step : (slider->max - slider->min) / 20.0;

    if (key == PNL_KEY_CLICK)
        return pnl_slider_click(slider, app);
    if (key == PNL_KEY_LEFT)
        pnl_slider_move(slider, slider->value - step);
    else if (key == PNL_KEY_RIGHT)
        pnl_slider_move(slider, slider->value + step);
    else if (key == PNL_KEY_HOME)
        pnl_slider_move(slider, slider->min);
    else if (key == PNL_KEY_END)
        pnl_slider_move(slider, slider->max);
    else
        return 0;
    return 1;
}

static const char *pnl_slider_hints(void *ctx)
{
    (void)ctx;
    return "left/right: adjust";
}

static void pnl_slider_release(void *ctx)
{
    struct pnl_slider_widget *slider = ctx;

    pnl_form_release(slider->app, slider->label, slider);
}

static int pnl_slider_set_label(pnl_widget_t *widget, const char *text)
{
    return pnl_widget_copy(widget->app, &((struct pnl_slider_widget *)widget->ctx)->label, text);
}

static const char *pnl_slider_label(const pnl_widget_t *widget)
{
    return ((const struct pnl_slider_widget *)widget->ctx)->label;
}

static int pnl_slider_set(pnl_widget_t *widget, double value)
{
    struct pnl_slider_widget *slider = widget->ctx;

    slider->value = pnl_slider_fit(slider, value);
    pnl_glide_to(slider->app, &slider->glide, pnl_slider_share(slider, slider->value), PNL_MOTION_COUNT);
    widget->app->dirty = 1;
    return PNL_OK;
}

static int pnl_slider_get(const pnl_widget_t *widget, double *out_value)
{
    *out_value = ((const struct pnl_slider_widget *)widget->ctx)->value;
    return PNL_OK;
}

static int pnl_slider_range(pnl_widget_t *widget, double min, double max)
{
    struct pnl_slider_widget *slider = widget->ctx;

    if (!(max > min))
        return PNL_ERR_ARG;
    slider->min = min;
    slider->max = max;
    return pnl_slider_set(widget, slider->value);
}

int pnl_widget_slider(pnl_app_t *app, const char *label, double min, double max, double step,
                      pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t slider_class = {pnl_slider_draw, pnl_slider_key, pnl_field_line, pnl_slider_hints,
                                                    pnl_slider_release};
    static const struct pnl_widget_ops slider_ops = {pnl_slider_set_label, pnl_slider_label, pnl_slider_set, pnl_slider_get,
                                                     pnl_slider_range};
    struct pnl_slider_widget *slider;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || !(max > min) || !(step >= 0.0))
        return PNL_ERR_ARG;
    slider = pnl_form_alloc(app, sizeof(*slider));
    if (slider != NULL)
        slider->app = app;
    if (slider == NULL || pnl_widget_copy(app, &slider->label, label) != PNL_OK) {
        if (slider != NULL)
            pnl_slider_release(slider);
        return PNL_ERR_NOMEM;
    }
    slider->app = app;
    slider->min = min;
    slider->max = max;
    slider->step = step;
    slider->value = min;
    return pnl_form_finish(app, PNL_WIDGET_SLIDER, &slider_class, &slider_ops, slider, &slider->self, out_widget);
}

static void pnl_button_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_button_widget *button = ctx;
    int focused = pnl_canvas_focused(canvas);
    int style = focused ? PNL_BOLD | PNL_REVERSE | PNL_FG(PNL_ROLE_ACCENT) : PNL_BG(PNL_ROLE_SURFACE);
    char text[PNL_CELL_MAX];

    if (button->pressed_ms > 0 && pnl_motion_on(app, PNL_MOTION_FEEDBACK) &&
        pnl_app_tween(app, button->pressed_ms, PNL_PRESS_MS) < 1.0)
        style = PNL_BOLD | PNL_REVERSE | PNL_FG(PNL_ROLE_OK);
    snprintf(text, sizeof(text), " %s ", button->label != NULL ? button->label : "");
    pnl_canvas_text(canvas, 0, 0, style, text);
}

static int pnl_button_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_button_widget *button = ctx;

    if (key != PNL_KEY_ENTER && key != ' ' && key != PNL_KEY_CLICK)
        return 0;
    button->pressed_ms = pnl_term_now_ms();
    app->dirty = 1;
    if (button->action_fn != NULL)
        button->action_fn(button->action_ctx, app);
    return 1;
}

static const char *pnl_button_hints(void *ctx)
{
    (void)ctx;
    return "enter: press";
}

static void pnl_button_release(void *ctx)
{
    struct pnl_button_widget *button = ctx;

    pnl_form_release(button->app, button->label, button);
}

int pnl_widget_button(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx,
                      pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t button_class = {pnl_button_draw, pnl_button_key, pnl_field_line, pnl_button_hints,
                                                    pnl_button_release};
    static const struct pnl_widget_ops button_ops = {pnl_button_set_label, pnl_button_label, NULL, NULL, NULL};
    struct pnl_button_widget *button;

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || out_widget == NULL || label == NULL)
        return PNL_ERR_ARG;
    button = pnl_form_alloc(app, sizeof(*button));
    if (button != NULL)
        button->app = app;
    if (button == NULL || pnl_widget_copy(app, &button->label, label) != PNL_OK) {
        if (button != NULL)
            pnl_button_release(button);
        return PNL_ERR_NOMEM;
    }
    button->app = app;
    button->action_fn = action_fn;
    button->action_ctx = action_ctx;
    return pnl_form_finish(app, PNL_WIDGET_BUTTON, &button_class, &button_ops, button, &button->self, out_widget);
}

