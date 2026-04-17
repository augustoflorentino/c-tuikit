#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_TABLE_GAP 2
#define PNL_TABLE_COLUMN_MAX 40
#define PNL_TABLE_MEASURE_ROWS 2000
#define PNL_TABLE_SYNC_ROWS 20000
#define PNL_TABLE_CHECK_EVERY 4096
#define PNL_TABLE_FILTER_MAX 128
#define PNL_TABLE_FOOTER "up/down: move  left/right: column  s: sort  /: filter  q: back"
#define PNL_TABLE_FILTER_FOOTER "type to filter  up/down: move  enter: keep  esc: clear"

struct pnl_table_job {
    char filter[PNL_TABLE_FILTER_MAX];
    size_t column;
    int descending;
    int sorted;
};

struct pnl_table {
    pnl_app_t *app;
    size_t columns;
    size_t rows;
    char **headers;
    char **cells;
    pnl_cell_fn cell_fn;
    pnl_release_fn release_fn;
    void *ctx;
    size_t *order;
    size_t order_len;
    size_t *work;
    size_t work_len;
    size_t *scratch;
    int *widths;
    int *flags;
    unsigned char *hidden;
    size_t hidden_len;
    size_t selected;
    size_t top;
    size_t column;
    struct pnl_table_job want;
    struct pnl_table_job job;
    struct pnl_edit edit;
    int editing;
    const char *footer;
    struct pnl_lock lock;
    int lock_ready;
    struct pnl_thread thread;
    int busy;
    int done;
    int cancel;
};

struct pnl_array_cells {
    const char *const *cells;
    size_t columns;
};

static const char *pnl_table_cell(const struct pnl_table *table, size_t row, size_t column, char *buf)
{
    const char *text;

    buf[0] = '\0';
    text = table->cells != NULL ? table->cells[row * table->columns + column]
                                : table->cell_fn(table->ctx, row, column, buf, PNL_CELL_MAX);

    return text != NULL ? text : "";
}

static int pnl_table_cancelled(struct pnl_table *table, size_t step)
{
    int cancel;

    if (step % PNL_TABLE_CHECK_EVERY != 0)
        return 0;
    pnl_lock(&table->lock);
    cancel = table->cancel;
    pnl_unlock(&table->lock);
    return cancel;
}

static int pnl_table_number(const char *text, double *out)
{
    char *end;

    if (text[0] == '\0')
        return 0;
    *out = strtod(text, &end);
    while (*end == ' ' || *end == '%')
        end++;
    return *end == '\0';
}

static int pnl_table_compare(const struct pnl_table *table, size_t a, size_t b)
{
    char left_buf[PNL_CELL_MAX];
    char right_buf[PNL_CELL_MAX];
    const char *left = pnl_table_cell(table, a, table->job.column, left_buf);
    const char *right = pnl_table_cell(table, b, table->job.column, right_buf);
    double x;
    double y;
    int result;

    if (pnl_table_number(left, &x) && pnl_table_number(right, &y))
        result = x < y ? -1 : x > y;
    else
        result = strcmp(left, right);
    return table->job.descending ? -result : result;
}

static int pnl_table_merge(struct pnl_table *table, size_t from, size_t middle, size_t to)
{
    size_t i = from;
    size_t j = middle;
    size_t k = from;

    while (i < middle && j < to) {
        if (pnl_table_cancelled(table, k))
            return 0;
        table->scratch[k++] = pnl_table_compare(table, table->work[j], table->work[i]) < 0 ? table->work[j++]
                                                                                          : table->work[i++];
    }
    while (i < middle)
        table->scratch[k++] = table->work[i++];
    while (j < to)
        table->scratch[k++] = table->work[j++];
    memcpy(table->work + from, table->scratch + from, (to - from) * sizeof(*table->work));
    return 1;
}

static int pnl_table_sort(struct pnl_table *table)
{
    size_t len = table->work_len;
    size_t width;
    size_t from;

    for (width = 1; width < len; width *= 2) {
        for (from = 0; from + width < len; from += 2 * width) {
            size_t to = from + 2 * width < len ? from + 2 * width : len;

            if (!pnl_table_merge(table, from, from + width, to))
                return 0;
        }
    }
    return 1;
}

static int pnl_table_matches(const struct pnl_table *table, size_t row)
{
    char buf[PNL_CELL_MAX];
    size_t c;

    for (c = 0; c < table->columns; c++) {
        if (pnl_text_contains(pnl_table_cell(table, row, c, buf), table->job.filter))
            return 1;
    }
    return 0;
}

static int pnl_table_filter(struct pnl_table *table)
{
    size_t i;

    table->work_len = 0;
    for (i = 0; i < table->rows; i++) {
        if (pnl_table_cancelled(table, i))
            return 0;
        if (table->job.filter[0] == '\0' || pnl_table_matches(table, i))
            table->work[table->work_len++] = i;
    }
    return 1;
}

static void pnl_table_compute(struct pnl_table *table)
{
    if (pnl_table_filter(table) && table->job.sorted)
        pnl_table_sort(table);
}

static void pnl_table_worker(void *arg)
{
    struct pnl_table *table = arg;

    pnl_table_compute(table);
    pnl_lock(&table->lock);
    table->done = 1;
    pnl_unlock(&table->lock);
    pnl_term_wake(&table->app->term);
}

static size_t pnl_table_find(const struct pnl_table *table, size_t row)
{
    size_t i;

    for (i = 0; i < table->order_len; i++) {
        if (table->order[i] == row)
            return i;
    }
    return 0;
}

static void pnl_table_apply(struct pnl_table *table)
{
    size_t *swap = table->order;
    int kept = table->selected < table->order_len;
    size_t row = kept ? table->order[table->selected] : 0;

    table->order = table->work;
    table->order_len = table->work_len;
    table->work = swap;
    table->selected = kept ? pnl_table_find(table, row) : 0;
    table->app->dirty = 1;
}

static void pnl_table_finish(struct pnl_table *table)
{
    int cancelled;

    pnl_thread_join(&table->thread);
    table->busy = 0;
    cancelled = table->cancel;
    table->cancel = 0;
    table->done = 0;
    if (!cancelled)
        pnl_table_apply(table);
}

static void pnl_table_stop(struct pnl_table *table)
{
    if (!table->busy)
        return;
    pnl_lock(&table->lock);
    table->cancel = 1;
    pnl_unlock(&table->lock);
    pnl_table_finish(table);
}

static int pnl_table_buffers(struct pnl_table *table)
{
    size_t count = table->rows ? table->rows : 1;

    if (table->work == NULL)
        table->work = pnl_mem_alloc(&table->app->mem, count * sizeof(*table->work));
    if (table->scratch == NULL)
        table->scratch = pnl_mem_alloc(&table->app->mem, count * sizeof(*table->scratch));
    return table->work != NULL && table->scratch != NULL;
}

static void pnl_table_start(struct pnl_table *table)
{
    pnl_table_stop(table);
    if (!pnl_table_buffers(table))
        return;
    table->job = table->want;
    if (table->rows > PNL_TABLE_SYNC_ROWS && pnl_thread_start(&table->thread, pnl_table_worker, table) == PNL_OK) {
        table->busy = 1;
        return;
    }
    pnl_table_compute(table);
    pnl_table_apply(table);
}

static void pnl_table_poll(struct pnl_table *table)
{
    int done;

    if (!table->busy)
        return;
    pnl_lock(&table->lock);
    done = table->done;
    pnl_unlock(&table->lock);
    if (done)
        pnl_table_finish(table);
}

static void pnl_table_widen(struct pnl_table *table, size_t row)
{
    char buf[PNL_CELL_MAX];
    size_t c;

    for (c = 0; c < table->columns; c++) {
        int width = pnl_text_width(pnl_table_cell(table, row, c, buf));

        if (width > table->widths[c])
            table->widths[c] = width > PNL_TABLE_COLUMN_MAX ? PNL_TABLE_COLUMN_MAX : width;
    }
}

static int pnl_table_total(const struct pnl_table *table)
{
    int total = 1;
    size_t c;

    for (c = 0; c < table->columns; c++) {
        if (!table->hidden[c])
            total += table->widths[c] + PNL_TABLE_GAP;
    }
    return total;
}

static void pnl_table_fit(struct pnl_table *table, int width)
{
    size_t c = table->columns;

    memset(table->hidden, 0, table->columns);
    table->hidden_len = 0;
    while (c-- > 0 && pnl_table_total(table) > width) {
        if (table->flags[c] & PNL_COLUMN_OPTIONAL) {
            table->hidden[c] = 1;
            table->hidden_len++;
        }
    }
    while (table->column > 0 && table->hidden[table->column])
        table->column--;
}

static int pnl_table_text(pnl_canvas_t *canvas, const struct pnl_table *table, size_t c, int x, int y, int style,
                          const char *text)
{
    int pad = table->flags[c] & PNL_COLUMN_RIGHT ? table->widths[c] - pnl_text_width(text) : 0;

    return pnl_canvas_clip(canvas, x + (pad > 0 ? pad : 0), y, table->widths[c], style, text);
}

static void pnl_table_marker(pnl_canvas_t *canvas, const struct pnl_table *table, size_t c, int end)
{
    const char *marker = table->want.descending ? "v" : "^";

    if (table->flags[c] & PNL_COLUMN_RIGHT)
        end -= pnl_text_width(table->headers[c]) + 3;
    pnl_canvas_text(canvas, end + 1, 0, PNL_FG(PNL_ROLE_MUTED), marker);
}

static void pnl_table_draw_header(struct pnl_table *table, pnl_canvas_t *canvas)
{
    char more[24];
    size_t c;
    int x = 1;

    for (c = 0; c < table->columns; c++) {
        int style = PNL_BOLD | (c == table->column ? PNL_UNDERLINE : 0);
        int end;

        if (table->hidden[c])
            continue;
        end = pnl_table_text(canvas, table, c, x, 0, style, table->headers[c]);
        if (table->want.sorted && c == table->want.column)
            pnl_table_marker(canvas, table, c, end);
        x += table->widths[c] + PNL_TABLE_GAP;
    }
    if (table->hidden_len == 0)
        return;
    snprintf(more, sizeof(more), "+%zu", table->hidden_len);
    pnl_canvas_text(canvas, pnl_canvas_width(canvas) - (int)strlen(more), 0, PNL_FG(PNL_ROLE_MUTED), more);
}

static void pnl_table_draw_filter(struct pnl_table *table, pnl_canvas_t *canvas)
{
    char count[64];
    int width = pnl_canvas_width(canvas);

    if (table->busy)
        snprintf(count, sizeof(count), "%s...", table->job.filter[0] != '\0' ? "filtering" : "sorting");
    else
        snprintf(count, sizeof(count), "%zu of %zu", table->order_len, table->rows);
    if (!table->editing && table->want.filter[0] == '\0' && !table->busy)
        return;
    pnl_canvas_text(canvas, width - (int)strlen(count) - 1, 1, PNL_FG(PNL_ROLE_MUTED), count);
    if (!table->editing && table->want.filter[0] == '\0')
        return;
    pnl_canvas_text(canvas, 1, 1, PNL_FG(PNL_ROLE_ACCENT), "/");
    pnl_edit_draw(&table->edit, canvas, 3, 1, width - (int)strlen(count) - 5, table->editing);
}

static void pnl_table_draw_row(struct pnl_table *table, pnl_canvas_t *canvas, int y, size_t index)
{
    int selected = index == table->selected;
    size_t row = table->order[index];
    char buf[PNL_CELL_MAX];
    size_t c;
    int x = 1;

    if (selected)
        pnl_canvas_fill(canvas, 0, y, pnl_canvas_width(canvas), 1, PNL_REVERSE);
    for (c = 0; c < table->columns; c++) {
        if (table->hidden[c])
            continue;
        pnl_table_text(canvas, table, c, x, y, selected ? PNL_REVERSE : 0, pnl_table_cell(table, row, c, buf));
        pnl_canvas_fill(canvas, x + table->widths[c], y, PNL_TABLE_GAP, 1, selected ? PNL_REVERSE : 0);
        x += table->widths[c] + PNL_TABLE_GAP;
    }
}

static void pnl_table_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_table *table = ctx;
    int rows = pnl_canvas_height(canvas) - 2;
    size_t end;
    size_t i;

    (void)app;
    pnl_table_poll(table);
    if (table->order_len > 0 && table->selected >= table->order_len)
        table->selected = table->order_len - 1;
    if (rows > 0 && table->selected < table->top)
        table->top = table->selected;
    if (rows > 0 && table->selected >= table->top + (size_t)rows)
        table->top = table->selected - (size_t)rows + 1;
    if (table->top > table->order_len)
        table->top = 0;
    end = rows > 0 && table->top + (size_t)rows < table->order_len ? table->top + (size_t)rows : table->order_len;
    for (i = table->top; i < end; i++)
        pnl_table_widen(table, table->order[i]);
    pnl_table_fit(table, pnl_canvas_width(canvas));
    pnl_table_draw_header(table, canvas);
    pnl_table_draw_filter(table, canvas);
    for (i = table->top; i < end; i++)
        pnl_table_draw_row(table, canvas, 2 + (int)(i - table->top), i);
    if (table->order_len == 0 && !table->busy && rows > 0 && table->want.filter[0] != '\0')
        pnl_canvas_text(canvas, 1, 2, PNL_FG(PNL_ROLE_MUTED), "no match");
}

static void pnl_table_move(struct pnl_table *table, int key)
{
    size_t step = key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN ? 10 : 1;

    if (table->order_len == 0)
        return;
    if (key == PNL_KEY_HOME)
        table->selected = 0;
    else if (key == PNL_KEY_END)
        table->selected = table->order_len - 1;
    else if (key == PNL_KEY_UP || key == 'k' || key == PNL_KEY_PAGE_UP)
        table->selected = table->selected > step ? table->selected - step : 0;
    else
        table->selected = table->selected + step < table->order_len ? table->selected + step : table->order_len - 1;
}

static void pnl_table_step_column(struct pnl_table *table, int direction)
{
    size_t c = table->column;

    while ((direction < 0 && c > 0) || (direction > 0 && c + 1 < table->columns)) {
        c = direction < 0 ? c - 1 : c + 1;
        if (!table->hidden[c]) {
            table->column = c;
            return;
        }
    }
}

static void pnl_table_sort_column(struct pnl_table *table)
{
    table->want.descending = table->want.sorted && table->want.column == table->column && !table->want.descending;
    table->want.column = table->column;
    table->want.sorted = 1;
    pnl_table_start(table);
}

static void pnl_table_click(struct pnl_table *table, pnl_app_t *app)
{
    size_t c;
    int x;
    int y;
    int left = 1;

    pnl_mouse_position(app, &x, &y);
    if (y >= 2 && table->top + (size_t)(y - 2) < table->order_len)
        table->selected = table->top + (size_t)(y - 2);
    for (c = 0; y == 0 && c < table->columns; c++) {
        if (table->hidden[c])
            continue;
        if (x >= left && x < left + table->widths[c] + PNL_TABLE_GAP) {
            table->column = c;
            pnl_table_sort_column(table);
            return;
        }
        left += table->widths[c] + PNL_TABLE_GAP;
    }
}

static int pnl_table_filter_key(struct pnl_table *table, int key)
{
    if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN) {
        pnl_table_move(table, key);
        return 1;
    }
    if (key == PNL_KEY_ENTER || key == PNL_KEY_ESCAPE) {
        table->editing = 0;
        table->footer = PNL_TABLE_FOOTER;
        if (key == PNL_KEY_ENTER)
            return 1;
        table->want.filter[0] = '\0';
        pnl_edit_init(&table->edit, table->want.filter, sizeof(table->want.filter));
        pnl_table_start(table);
        return 1;
    }
    if (pnl_edit_key(&table->edit, key))
        pnl_table_start(table);
    return 1;
}

static int pnl_table_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_table *table = ctx;

    if (table->editing && !pnl_is_mouse_key(key))
        return pnl_table_filter_key(table, key);
    if (key == PNL_KEY_CLICK)
        pnl_table_click(table, app);
    else if (key == '/') {
        table->editing = 1;
        table->footer = PNL_TABLE_FILTER_FOOTER;
    } else if (key == PNL_KEY_LEFT || key == 'h' || key == PNL_KEY_RIGHT || key == 'l')
        pnl_table_step_column(table, key == PNL_KEY_LEFT || key == 'h' ? -1 : 1);
    else if (key == 's')
        pnl_table_sort_column(table);
    else if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == 'j' || key == 'k' ||
             (key >= PNL_KEY_PAGE_UP && key <= PNL_KEY_END))
        pnl_table_move(table, key);
    else
        return 0;
    return 1;
}

static void pnl_table_drop(pnl_app_t *app, void *ctx)
{
    struct pnl_table *table = ctx;
    size_t i;

    pnl_table_stop(table);
    if (table->release_fn != NULL)
        table->release_fn(table->ctx);
    for (i = 0; table->headers != NULL && i < table->columns; i++)
        pnl_mem_free(&app->mem, table->headers[i]);
    for (i = 0; table->cells != NULL && i < table->rows * table->columns; i++)
        pnl_mem_free(&app->mem, table->cells[i]);
    if (table->lock_ready)
        pnl_lock_destroy(&table->lock);
    pnl_mem_free(&app->mem, table->headers);
    pnl_mem_free(&app->mem, table->cells);
    pnl_mem_free(&app->mem, table->order);
    pnl_mem_free(&app->mem, table->work);
    pnl_mem_free(&app->mem, table->scratch);
    pnl_mem_free(&app->mem, table->widths);
    pnl_mem_free(&app->mem, table->flags);
    pnl_mem_free(&app->mem, table->hidden);
    pnl_mem_free(&app->mem, table);
}

static char **pnl_table_strings(pnl_app_t *app, const char *const *texts, size_t count)
{
    char **copy = pnl_mem_alloc(&app->mem, (count ? count : 1) * sizeof(*copy));
    size_t i;

    if (copy == NULL)
        return NULL;
    memset(copy, 0, (count ? count : 1) * sizeof(*copy));
    for (i = 0; i < count; i++) {
        copy[i] = pnl_mem_copy(&app->mem, texts[i] != NULL ? texts[i] : "");
        if (copy[i] == NULL)
            break;
    }
    if (i == count)
        return copy;
    while (i-- > 0)
        pnl_mem_free(&app->mem, copy[i]);
    pnl_mem_free(&app->mem, copy);
    return NULL;
}

static int pnl_table_prepare(struct pnl_table *table, const char *const *headers, const int *column_flags)
{
    pnl_app_t *app = table->app;
    size_t columns = table->columns;
    size_t i;

    table->headers = pnl_table_strings(app, headers, columns);
    table->order = pnl_mem_alloc(&app->mem, (table->rows ? table->rows : 1) * sizeof(*table->order));
    table->widths = pnl_mem_alloc(&app->mem, columns * sizeof(*table->widths));
    table->flags = pnl_mem_alloc(&app->mem, columns * sizeof(*table->flags));
    table->hidden = pnl_mem_alloc(&app->mem, columns);
    if (table->headers == NULL || table->order == NULL || table->widths == NULL || table->flags == NULL ||
        table->hidden == NULL)
        return PNL_ERR_NOMEM;
    if (pnl_lock_init(&table->lock) != PNL_OK)
        return PNL_ERR_NOMEM;
    table->lock_ready = 1;
    for (i = 0; i < columns; i++) {
        table->flags[i] = column_flags != NULL ? column_flags[i] : 0;
        table->widths[i] = pnl_text_width(table->headers[i]) + 2;
        if (table->widths[i] > PNL_TABLE_COLUMN_MAX)
            table->widths[i] = PNL_TABLE_COLUMN_MAX;
    }
    memset(table->hidden, 0, columns);
    for (i = 0; i < table->rows; i++)
        table->order[i] = i;
    table->order_len = table->rows;
    for (i = 0; i < table->rows && i < PNL_TABLE_MEASURE_ROWS; i++)
        pnl_table_widen(table, i);
    table->footer = PNL_TABLE_FOOTER;
    pnl_edit_init(&table->edit, table->want.filter, sizeof(table->want.filter));
    return PNL_OK;
}

static struct pnl_table *pnl_table_new(pnl_app_t *app, size_t columns, size_t rows)
{
    struct pnl_table *table = pnl_mem_alloc(&app->mem, sizeof(*table));

    if (table == NULL)
        return NULL;
    memset(table, 0, sizeof(*table));
    table->app = app;
    table->columns = columns;
    table->rows = rows;
    return table;
}

static int pnl_table_push(pnl_app_t *app, const char *title, struct pnl_table *table)
{
    struct pnl_view view;
    int status;

    memset(&view, 0, sizeof(view));
    view.footer_ref = &table->footer;
    view.draw_fn = pnl_table_draw;
    view.key_fn = pnl_table_key;
    view.drop_fn = pnl_table_drop;
    view.ctx = table;
    view.title = pnl_mem_copy(&app->mem, title);
    status = view.title == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &view);
    if (status != PNL_OK)
        pnl_mem_free(&app->mem, view.title);
    return status;
}

static const char *pnl_array_cell(void *ctx, size_t row, size_t column, char *buf, size_t buf_len)
{
    const struct pnl_array_cells *array = ctx;

    (void)buf;
    (void)buf_len;
    return array->cells[row * array->columns + column];
}

static int pnl_table_valid(const pnl_app_t *app, const char *title, const char *const *headers, size_t columns,
                           size_t rows)
{
    return app != NULL && title != NULL && headers != NULL && columns > 0 &&
           rows <= ((size_t)-1) / sizeof(char *) / (columns + 1) - 1 && (app->running || app->cli);
}

int pnl_table(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags,
              size_t columns, const char *const *cells, size_t rows)
{
    struct pnl_array_cells array;
    struct pnl_table *table;
    int status;

    if (!pnl_table_valid(app, title, headers, columns, rows) || (cells == NULL && rows > 0))
        return PNL_ERR_ARG;
    array.cells = cells;
    array.columns = columns;
    if (app->cli)
        return pnl_cli_table(app, headers, column_flags, columns, rows, pnl_array_cell, &array);
    table = pnl_table_new(app, columns, rows);
    if (table == NULL)
        return PNL_ERR_NOMEM;
    table->cells = pnl_table_strings(app, cells, rows * columns);
    status = table->cells == NULL ? PNL_ERR_NOMEM : pnl_table_prepare(table, headers, column_flags);
    if (status == PNL_OK)
        status = pnl_table_push(app, title, table);
    if (status != PNL_OK)
        pnl_table_drop(app, table);
    return status;
}

int pnl_table_source(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags,
                     size_t columns, size_t rows, pnl_cell_fn cell_fn, pnl_release_fn release_fn, void *ctx)
{
    struct pnl_table *table;
    int status;

    if (!pnl_table_valid(app, title, headers, columns, rows) || cell_fn == NULL)
        return PNL_ERR_ARG;
    if (app->cli) {
        status = pnl_cli_table(app, headers, column_flags, columns, rows, cell_fn, ctx);
        if (status == PNL_OK && release_fn != NULL)
            release_fn(ctx);
        return status;
    }
    table = pnl_table_new(app, columns, rows);
    if (table == NULL)
        return PNL_ERR_NOMEM;
    table->cell_fn = cell_fn;
    table->ctx = ctx;
    status = pnl_table_prepare(table, headers, column_flags);
    if (status == PNL_OK)
        status = pnl_table_push(app, title, table);
    if (status != PNL_OK) {
        pnl_table_drop(app, table);
        return status;
    }
    table->release_fn = release_fn;
    return PNL_OK;
}
