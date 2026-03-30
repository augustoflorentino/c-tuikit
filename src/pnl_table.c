#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_TABLE_GAP 2
#define PNL_TABLE_COLUMN_MAX 40
#define PNL_TABLE_FOOTER "up/down: move  left/right: column  s: sort  q: back"

struct pnl_table {
    char **cells;
    size_t columns;
    size_t rows;
    size_t *order;
    size_t *scratch;
    int *widths;
    int *flags;
    unsigned char *hidden;
    size_t hidden_len;
    size_t selected;
    size_t top;
    size_t column;
    size_t sorted_by;
    int descending;
    int sorted;
};

static const char *pnl_table_cell(const struct pnl_table *table, size_t row, size_t column)
{
    return table->cells[(row + 1) * table->columns + column];
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
    const char *left = pnl_table_cell(table, a, table->sorted_by);
    const char *right = pnl_table_cell(table, b, table->sorted_by);
    double x;
    double y;
    int result;

    if (pnl_table_number(left, &x) && pnl_table_number(right, &y))
        result = x < y ? -1 : x > y;
    else
        result = strcmp(left, right);
    return table->descending ? -result : result;
}

static void pnl_table_merge(struct pnl_table *table, size_t from, size_t middle, size_t to)
{
    size_t i = from;
    size_t j = middle;
    size_t k = from;

    while (i < middle && j < to)
        table->scratch[k++] = pnl_table_compare(table, table->order[j], table->order[i]) < 0 ? table->order[j++]
                                                                                            : table->order[i++];
    while (i < middle)
        table->scratch[k++] = table->order[i++];
    while (j < to)
        table->scratch[k++] = table->order[j++];
    memcpy(table->order + from, table->scratch + from, (to - from) * sizeof(*table->order));
}

static void pnl_table_sort(struct pnl_table *table)
{
    size_t width;
    size_t from;

    for (width = 1; width < table->rows; width *= 2) {
        for (from = 0; from + width < table->rows; from += 2 * width) {
            size_t to = from + 2 * width < table->rows ? from + 2 * width : table->rows;

            pnl_table_merge(table, from, from + width, to);
        }
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

static void pnl_table_draw_header(struct pnl_table *table, pnl_canvas_t *canvas)
{
    char more[16];
    size_t c;
    int x = 1;

    for (c = 0; c < table->columns; c++) {
        int style = PNL_BOLD | (c == table->column ? PNL_UNDERLINE : 0);
        int end;

        if (table->hidden[c])
            continue;
        end = pnl_table_text(canvas, table, c, x, 0, style, table->cells[c]);
        if (table->sorted && c == table->sorted_by)
            pnl_canvas_text(canvas, end, 0, PNL_FG(PNL_ROLE_MUTED), table->descending ? " v" : " ^");
        x += table->widths[c] + PNL_TABLE_GAP;
    }
    if (table->hidden_len == 0)
        return;
    snprintf(more, sizeof(more), "+%zu", table->hidden_len);
    pnl_canvas_text(canvas, pnl_canvas_width(canvas) - (int)strlen(more), 0, PNL_FG(PNL_ROLE_MUTED), more);
}

static void pnl_table_draw_row(struct pnl_table *table, pnl_canvas_t *canvas, int y, size_t index)
{
    int selected = index == table->selected;
    size_t row = table->order[index];
    size_t c;
    int x = 1;

    if (selected)
        pnl_canvas_fill(canvas, 0, y, pnl_canvas_width(canvas), 1, PNL_REVERSE);
    for (c = 0; c < table->columns; c++) {
        if (table->hidden[c])
            continue;
        pnl_table_text(canvas, table, c, x, y, selected ? PNL_REVERSE : 0, pnl_table_cell(table, row, c));
        pnl_canvas_fill(canvas, x + table->widths[c], y, PNL_TABLE_GAP, 1, selected ? PNL_REVERSE : 0);
        x += table->widths[c] + PNL_TABLE_GAP;
    }
}

static void pnl_table_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_table *table = ctx;
    int rows = pnl_canvas_height(canvas) - 2;
    size_t i;

    (void)app;
    pnl_table_fit(table, pnl_canvas_width(canvas));
    pnl_table_draw_header(table, canvas);
    if (rows <= 0)
        return;
    if (table->selected < table->top)
        table->top = table->selected;
    if (table->selected >= table->top + (size_t)rows)
        table->top = table->selected - (size_t)rows + 1;
    for (i = table->top; i < table->rows && i < table->top + (size_t)rows; i++)
        pnl_table_draw_row(table, canvas, 2 + (int)(i - table->top), i);
}

static void pnl_table_move(struct pnl_table *table, int key)
{
    size_t step = key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN ? 10 : 1;

    if (table->rows == 0)
        return;
    if (key == PNL_KEY_HOME)
        table->selected = 0;
    else if (key == PNL_KEY_END)
        table->selected = table->rows - 1;
    else if (key == PNL_KEY_UP || key == 'k' || key == PNL_KEY_PAGE_UP)
        table->selected = table->selected > step ? table->selected - step : 0;
    else
        table->selected = table->selected + step < table->rows ? table->selected + step : table->rows - 1;
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

static int pnl_table_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_table *table = ctx;

    (void)app;
    if (key == PNL_KEY_LEFT || key == 'h' || key == PNL_KEY_RIGHT || key == 'l')
        pnl_table_step_column(table, key == PNL_KEY_LEFT || key == 'h' ? -1 : 1);
    else if (key == 's') {
        table->descending = table->sorted && table->sorted_by == table->column && !table->descending;
        table->sorted_by = table->column;
        table->sorted = 1;
        pnl_table_sort(table);
    } else if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == 'j' || key == 'k' ||
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

    for (i = 0; table->cells != NULL && i < (table->rows + 1) * table->columns; i++)
        pnl_mem_free(&app->mem, table->cells[i]);
    pnl_mem_free(&app->mem, table->cells);
    pnl_mem_free(&app->mem, table->order);
    pnl_mem_free(&app->mem, table->scratch);
    pnl_mem_free(&app->mem, table->widths);
    pnl_mem_free(&app->mem, table->flags);
    pnl_mem_free(&app->mem, table->hidden);
    pnl_mem_free(&app->mem, table);
}

static int pnl_table_fill(pnl_app_t *app, struct pnl_table *table, const char *const *headers,
                          const char *const *cells)
{
    size_t total = (table->rows + 1) * table->columns;
    size_t i;

    for (i = 0; i < total; i++) {
        const char *text = i < table->columns ? headers[i] : cells[i - table->columns];
        int width;

        table->cells[i] = pnl_mem_copy(&app->mem, text ? text : "");
        if (table->cells[i] == NULL)
            return PNL_ERR_NOMEM;
        width = pnl_text_width(table->cells[i]) + (i < table->columns ? 2 : 0);
        if (width > table->widths[i % table->columns])
            table->widths[i % table->columns] = width > PNL_TABLE_COLUMN_MAX ? PNL_TABLE_COLUMN_MAX : width;
    }
    for (i = 0; i < table->rows; i++)
        table->order[i] = i;
    return PNL_OK;
}

static struct pnl_table *pnl_table_create(pnl_app_t *app, const char *const *headers, const int *column_flags,
                                          size_t columns, const char *const *cells, size_t rows)
{
    struct pnl_table *table = pnl_mem_alloc(&app->mem, sizeof(*table));

    if (table == NULL)
        return NULL;
    memset(table, 0, sizeof(*table));
    table->columns = columns;
    table->rows = rows;
    table->cells = pnl_mem_alloc(&app->mem, (rows + 1) * columns * sizeof(*table->cells));
    table->order = pnl_mem_alloc(&app->mem, (rows ? rows : 1) * sizeof(*table->order));
    table->scratch = pnl_mem_alloc(&app->mem, (rows ? rows : 1) * sizeof(*table->scratch));
    table->widths = pnl_mem_alloc(&app->mem, columns * sizeof(*table->widths));
    table->flags = pnl_mem_alloc(&app->mem, columns * sizeof(*table->flags));
    table->hidden = pnl_mem_alloc(&app->mem, columns);
    if (table->flags != NULL && column_flags != NULL)
        memcpy(table->flags, column_flags, columns * sizeof(*table->flags));
    else if (table->flags != NULL)
        memset(table->flags, 0, columns * sizeof(*table->flags));
    if (table->hidden != NULL)
        memset(table->hidden, 0, columns);
    if (table->cells != NULL)
        memset(table->cells, 0, (rows + 1) * columns * sizeof(*table->cells));
    if (table->widths != NULL)
        memset(table->widths, 0, columns * sizeof(*table->widths));
    if (table->cells == NULL || table->order == NULL || table->scratch == NULL || table->widths == NULL ||
        table->flags == NULL || table->hidden == NULL ||
        pnl_table_fill(app, table, headers, cells) != PNL_OK) {
        pnl_table_drop(app, table);
        return NULL;
    }
    return table;
}

int pnl_table(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags,
              size_t columns, const char *const *cells, size_t rows)
{
    struct pnl_table *table;
    struct pnl_view view;
    int status;

    if (app == NULL || title == NULL || headers == NULL || columns == 0 || (cells == NULL && rows > 0) ||
        rows > ((size_t)-1) / sizeof(char *) / (columns + 1) - 1 || (!app->running && !app->cli))
        return PNL_ERR_ARG;
    if (app->cli)
        return pnl_cli_table(app, headers, column_flags, columns, cells, rows);
    table = pnl_table_create(app, headers, column_flags, columns, cells, rows);
    if (table == NULL)
        return PNL_ERR_NOMEM;
    memset(&view, 0, sizeof(view));
    view.footer = PNL_TABLE_FOOTER;
    view.draw_fn = pnl_table_draw;
    view.key_fn = pnl_table_key;
    view.drop_fn = pnl_table_drop;
    view.ctx = table;
    view.title = pnl_mem_copy(&app->mem, title);
    status = view.title == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &view);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, view.title);
        pnl_table_drop(app, table);
    }
    return status;
}
