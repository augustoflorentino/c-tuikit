#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <string.h>

static const char *const regions[] = {"sao-paulo", "virginia", "frankfurt", "tokyo"};
static const char *const sizes[] = {"small", "medium", "large"};

struct deploy_form {
    pnl_widget_t *name;
    pnl_widget_t *token;
    pnl_widget_t *region;
    pnl_widget_t *size;
    pnl_widget_t *replicas;
    pnl_widget_t *backups;
    pnl_widget_t *public_access;
    pnl_widget_t *deploy;
    pnl_widget_t *reset;
    pnl_widget_t *buttons;
    pnl_widget_t *fields;
    pnl_widget_t *summary;
    pnl_widget_t *gap;
    pnl_layout_t *layout;
};

static double value_of(const pnl_widget_t *widget)
{
    double value = 0.0;

    pnl_widget_get_value(widget, &value);
    return value;
}

static void describe(void *ctx, pnl_app_t *app, pnl_widget_t *widget)
{
    struct deploy_form *form = ctx;
    const char *name = pnl_widget_get_text(form->name);
    char text[512];

    (void)app;
    (void)widget;
    snprintf(text, sizeof(text),
             "service   %s\nregion    %s\nsize      %s\nreplicas  %.0f\nbackups   %s\naccess    %s\ntoken     %s",
             name[0] != '\0' ? name : "(no name yet)", pnl_widget_get_text(form->region),
             pnl_widget_get_text(form->size), value_of(form->replicas), value_of(form->backups) ? "daily" : "none",
             value_of(form->public_access) ? "public" : "private",
             pnl_widget_get_text(form->token)[0] != '\0' ? "set" : "missing");
    pnl_widget_set_text(form->summary, text);
}

static void deploy(void *ctx, pnl_app_t *app)
{
    struct deploy_form *form = ctx;
    char text[128];

    if (pnl_widget_get_text(form->name)[0] == '\0') {
        pnl_widget_focus(form->fields, form->name);
        pnl_view_shake(app);
        pnl_toast(app, PNL_MARK_FAIL, "the service needs a name");
        return;
    }
    snprintf(text, sizeof(text), "%s deployed to %s", pnl_widget_get_text(form->name),
             pnl_widget_get_text(form->region));
    pnl_toast(app, PNL_MARK_OK, text);
}

static void reset(void *ctx, pnl_app_t *app)
{
    struct deploy_form *form = ctx;

    pnl_widget_set_text(form->name, "");
    pnl_widget_set_text(form->token, "");
    pnl_widget_set_value(form->region, 0);
    pnl_widget_set_value(form->size, 1);
    pnl_widget_set_value(form->replicas, 2);
    pnl_widget_set_value(form->backups, 1);
    pnl_widget_set_value(form->public_access, 0);
    pnl_widget_focus(form->fields, form->name);
    describe(form, app, NULL);
}

static int create_fields(pnl_app_t *app, struct deploy_form *form)
{
    int status = pnl_widget_input(app, "name     ", 0, &form->name);

    if (status == PNL_OK)
        status = pnl_widget_input(app, "token    ", PNL_INPUT_SECRET, &form->token);
    if (status == PNL_OK)
        status = pnl_widget_choice(app, "region   ", regions, 4, &form->region);
    if (status == PNL_OK)
        status = pnl_widget_choice(app, "size     ", sizes, 3, &form->size);
    if (status == PNL_OK)
        status = pnl_widget_slider(app, "replicas ", 1, 12, 1, &form->replicas);
    if (status == PNL_OK)
        status = pnl_widget_check(app, "daily backups", 1, &form->backups);
    if (status == PNL_OK)
        status = pnl_widget_check(app, "public access", 0, &form->public_access);
    if (status == PNL_OK)
        status = pnl_widget_button(app, "Deploy", deploy, form, &form->deploy);
    if (status == PNL_OK)
        status = pnl_widget_button(app, "Reset", reset, form, &form->reset);
    if (status == PNL_OK)
        status = pnl_widget_text(app, "", 0, &form->summary);
    if (status == PNL_OK)
        status = pnl_widget_text(app, "", 0, &form->gap);
    return status;
}

static int arrange(pnl_app_t *app, struct deploy_form *form)
{
    int status = pnl_widget_stack(app, PNL_STACK_COLUMNS, &form->buttons);
    pnl_widget_t *rows[] = {form->name,     form->token,   form->region,        form->size,
                            form->replicas, form->backups, form->public_access, form->buttons};
    size_t i;

    if (status == PNL_OK)
        status = pnl_widget_add(form->buttons, form->deploy, 10);
    if (status == PNL_OK)
        status = pnl_widget_add(form->buttons, form->reset, 9);
    if (status == PNL_OK)
        status = pnl_widget_stack(app, PNL_STACK_ROWS, &form->fields);
    for (i = 0; i < 8 && status == PNL_OK; i++) {
        status = pnl_widget_add(form->fields, rows[i], 0);
        if (status == PNL_OK && (i == 1 || i == 4 || i == 6))
            status = pnl_widget_add(form->fields, form->gap, 0);
    }
    return status;
}

static int connect_changes(struct deploy_form *form)
{
    pnl_widget_t *fields[] = {form->name,     form->token,   form->region,       form->size,
                              form->replicas, form->backups, form->public_access};
    size_t i;

    for (i = 0; i < 7; i++) {
        int status = pnl_widget_on_change(fields[i], describe, form);

        if (status != PNL_OK)
            return status;
    }
    return PNL_OK;
}

static void open_form(void *ctx, pnl_app_t *app)
{
    struct deploy_form *form = ctx;

    pnl_view_push_layout(app, "new service", form->layout);
}

static void destroy_form(struct deploy_form *form)
{
    pnl_layout_destroy(form->layout);
    pnl_widget_destroy(form->fields);
    pnl_widget_destroy(form->buttons);
    pnl_widget_destroy(form->name);
    pnl_widget_destroy(form->token);
    pnl_widget_destroy(form->region);
    pnl_widget_destroy(form->size);
    pnl_widget_destroy(form->replicas);
    pnl_widget_destroy(form->backups);
    pnl_widget_destroy(form->public_access);
    pnl_widget_destroy(form->deploy);
    pnl_widget_destroy(form->reset);
    pnl_widget_destroy(form->summary);
    pnl_widget_destroy(form->gap);
}

int main(int argc, char **argv)
{
    struct deploy_form form;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int node = 0;
    int status;

    memset(&form, 0, sizeof(form));
    status = pnl_app_create("form", NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_enable_motion(app, PNL_MOTION_SLIDE | PNL_MOTION_ENTER | PNL_MOTION_FEEDBACK |
                                                PNL_MOTION_COUNT);
    if (status == PNL_OK)
        status = create_fields(app, &form);
    if (status == PNL_OK)
        status = arrange(app, &form);
    if (status == PNL_OK)
        status = connect_changes(&form);
    if (status == PNL_OK)
        status = pnl_layout_create(app, PNL_SPLIT_COLUMNS, &form.layout);
    if (status == PNL_OK)
        status = pnl_layout_widget(form.layout, PNL_LAYOUT_ROOT, "service", form.fields, &node);
    if (status == PNL_OK)
        status = pnl_layout_size(form.layout, node, 44, 40);
    if (status == PNL_OK)
        status = pnl_layout_widget(form.layout, PNL_LAYOUT_ROOT, "summary", form.summary, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "New service", open_form, &form);
    if (status == PNL_OK) {
        reset(&form, app);
        status = pnl_app_run(app, argc, argv, &exit_code);
    }
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    destroy_form(&form);
    pnl_app_destroy(app);
    return exit_code;
}
