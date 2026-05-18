#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_VALUE_TEXT 256

struct pnl_value {
    pnl_app_t *app;
    struct pnl_lock lock;
    char *label;
    int mark;
    double meter;
    int pending;
    char text[PNL_VALUE_TEXT];
};

int pnl_value_create(pnl_app_t *app, const char *label, pnl_value_t **out_value)
{
    pnl_value_t *value;
    int status;

    if (out_value != NULL)
        *out_value = NULL;
    if (app == NULL || label == NULL || out_value == NULL)
        return PNL_ERR_ARG;
    status = pnl_term_wake_open(&app->term);
    if (status != PNL_OK)
        return status;
    status = pnl_mem_grow(&app->mem, (void **)&app->values, &app->values_cap, app->values_len + 1,
                          sizeof(*app->values));
    if (status != PNL_OK)
        return status;
    value = pnl_mem_alloc(&app->mem, sizeof(*value));
    if (value == NULL)
        return PNL_ERR_NOMEM;
    memset(value, 0, sizeof(*value));
    value->app = app;
    value->mark = PNL_MARK_WAIT;
    value->meter = -1.0;
    value->label = pnl_mem_copy(&app->mem, label);
    status = value->label == NULL ? PNL_ERR_NOMEM : pnl_lock_init(&value->lock);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, value->label);
        pnl_mem_free(&app->mem, value);
        return status;
    }
    app->values[app->values_len++] = value;
    *out_value = value;
    return PNL_OK;
}

void pnl_value_destroy(pnl_value_t *value)
{
    pnl_app_t *app;
    size_t i;

    if (value == NULL)
        return;
    app = value->app;
    for (i = 0; i < app->values_len && app->values[i] != value; i++)
        ;
    if (i < app->values_len) {
        memmove(app->values + i, app->values + i + 1, (app->values_len - i - 1) * sizeof(*app->values));
        app->values_len--;
    }
    pnl_lock_destroy(&value->lock);
    pnl_mem_free(&app->mem, value->label);
    pnl_mem_free(&app->mem, value);
    app->dirty = 1;
}

static void pnl_value_store(pnl_value_t *value, int mark, double meter, const char *text)
{
    size_t len = strlen(text);
    int wake;

    if (len >= PNL_VALUE_TEXT) {
        len = PNL_VALUE_TEXT - 1;
        while (len > 0 && ((unsigned char)text[len] & 0xc0) == 0x80)
            len--;
    }

    pnl_lock(&value->lock);
    wake = !value->pending && (value->mark != mark || value->meter != meter || strncmp(value->text, text, len) != 0 ||
                               value->text[len] != '\0');
    value->pending = value->pending || wake;
    value->mark = mark;
    value->meter = meter;
    memcpy(value->text, text, len);
    value->text[len] = '\0';
    pnl_unlock(&value->lock);
    if (wake)
        pnl_term_wake(&value->app->term);
}

int pnl_value_set(pnl_value_t *value, int mark, const char *text)
{
    if (value == NULL || text == NULL || !pnl_valid_mark(mark))
        return PNL_ERR_ARG;
    pnl_value_store(value, mark, -1.0, text);
    return PNL_OK;
}

int pnl_value_set_meter(pnl_value_t *value, int mark, double fraction)
{
    if (value == NULL || !pnl_valid_mark(mark) || fraction != fraction)
        return PNL_ERR_ARG;
    pnl_value_store(value, mark, fraction < 0.0 ? 0.0 : fraction > 1.0 ? 1.0 : fraction, "");
    return PNL_OK;
}

void pnl_values_lines(pnl_app_t *app)
{
    char text[PNL_VALUE_TEXT];
    size_t i;

    app->in_status = 1;
    for (i = 0; i < app->values_len; i++) {
        pnl_value_t *value = app->values[i];
        double meter;
        int mark;

        pnl_lock(&value->lock);
        value->pending = 0;
        mark = value->mark;
        meter = value->meter;
        memcpy(text, value->text, sizeof(text));
        pnl_unlock(&value->lock);
        if (pnl_status_line(app, mark, value->label, text) == PNL_OK)
            app->lines[app->lines_len - 1].meter = meter;
    }
    app->in_status = 0;
}
