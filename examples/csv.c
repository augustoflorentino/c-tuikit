#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLUMNS_MAX 32

struct sheet {
    const char *path;
    char *data;
    size_t size;
    size_t *starts;
    size_t rows;
    size_t columns;
    char *headers[COLUMNS_MAX];
};

static const char *field(const struct sheet *sheet, size_t line, size_t column, char *buf, size_t buf_len)
{
    const char *at = sheet->data + sheet->starts[line];
    const char *end = sheet->data + sheet->starts[line + 1];
    size_t len = 0;
    int quoted = 0;

    for (; at < end && column > 0; at++) {
        if (*at == '"')
            quoted = !quoted;
        else if (*at == ',' && !quoted)
            column--;
    }
    quoted = 0;
    for (; at < end && *at != '\n' && *at != '\r' && len + 1 < buf_len; at++) {
        if (*at == '"' && quoted && at + 1 < end && at[1] == '"')
            buf[len++] = *++at;
        else if (*at == '"')
            quoted = !quoted;
        else if (*at == ',' && !quoted)
            break;
        else
            buf[len++] = *at;
    }
    buf[len] = '\0';
    return buf;
}

static const char *cell(void *ctx, size_t row, size_t column, char *buf, size_t buf_len)
{
    return field(ctx, row + 1, column, buf, buf_len);
}

static int read_file(struct sheet *sheet)
{
    FILE *file = fopen(sheet->path, "rb");
    long size;
    int ok;

    if (file == NULL)
        return 0;
    ok = fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0;
    if (ok) {
        sheet->size = (size_t)size;
        sheet->data = malloc(sheet->size + 1);
        ok = sheet->data != NULL && fread(sheet->data, 1, sheet->size, file) == sheet->size;
    }
    fclose(file);
    return ok;
}

static int load(struct sheet *sheet)
{
    size_t lines = 0;
    size_t i;

    if (!read_file(sheet))
        return 0;
    for (i = 0; i < sheet->size; i++)
        lines += sheet->data[i] == '\n';
    sheet->starts = malloc((lines + 2) * sizeof(*sheet->starts));
    if (sheet->starts == NULL)
        return 0;
    sheet->starts[0] = 0;
    for (i = 0; i < sheet->size; i++) {
        if (sheet->data[i] == '\n' && i + 1 < sheet->size)
            sheet->starts[++sheet->rows] = i + 1;
    }
    sheet->starts[sheet->rows + 1] = sheet->size;
    return 1;
}

static char *copy_text(const char *text)
{
    char *copy = malloc(strlen(text) + 1);

    if (copy != NULL)
        strcpy(copy, text);
    return copy;
}

static int read_headers(struct sheet *sheet)
{
    char buf[256];

    sheet->columns = 1;
    for (size_t i = sheet->starts[0]; i < sheet->starts[1] && sheet->data[i] != '\n'; i++)
        sheet->columns += sheet->data[i] == ',';
    if (sheet->columns > COLUMNS_MAX)
        sheet->columns = COLUMNS_MAX;
    for (size_t c = 0; c < sheet->columns; c++) {
        sheet->headers[c] = copy_text(field(sheet, 0, c, buf, sizeof(buf)));
        if (sheet->headers[c] == NULL)
            return 0;
    }
    return 1;
}

static void open_table(void *ctx, pnl_app_t *app)
{
    struct sheet *sheet = ctx;

    pnl_table_source(app, sheet->path, (const char *const *)sheet->headers, NULL, sheet->columns, sheet->rows, cell,
                     NULL, sheet);
}

static void show_status(void *ctx, pnl_app_t *app)
{
    struct sheet *sheet = ctx;
    char text[96];

    snprintf(text, sizeof(text), "%zu rows, %zu columns, %.1f MB", sheet->rows, sheet->columns,
             (double)sheet->size / (1024.0 * 1024.0));
    pnl_status_line(app, PNL_MARK_OK, "file", sheet->path);
    pnl_status_line(app, PNL_MARK_OK, "size", text);
}

static void release(struct sheet *sheet)
{
    for (size_t c = 0; c < COLUMNS_MAX; c++)
        free(sheet->headers[c]);
    free(sheet->starts);
    free(sheet->data);
}

int main(int argc, char **argv)
{
    struct sheet sheet;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int status;

    if (argc < 2 || argv[1][0] == '-') {
        fprintf(stderr, "usage: %s FILE.csv [command]\n", argv[0]);
        return 2;
    }
    memset(&sheet, 0, sizeof(sheet));
    sheet.path = argv[1];
    if (!load(&sheet) || !read_headers(&sheet)) {
        fprintf(stderr, "%s: cannot read\n", sheet.path);
        release(&sheet);
        return 1;
    }
    argv[1] = argv[0];
    status = pnl_app_create("csv", NULL, NULL, NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, show_status, &sheet);
    if (status == PNL_OK)
        status = pnl_app_set_memory(app, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Table", open_table, &sheet);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc - 1, argv + 1, &exit_code);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    pnl_app_destroy(app);
    release(&sheet);
    return exit_code;
}
