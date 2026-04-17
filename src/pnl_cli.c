#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_EXIT_OK 0
#define PNL_EXIT_FAIL 1
#define PNL_EXIT_USAGE 2
#define PNL_COMMAND_MAX 64

static const char *const pnl_mark_names[] = {"ok", "..", "!!"};
static const char *const pnl_mark_json[] = {"ok", "wait", "fail"};
static const char *const pnl_mark_colors[] = {"\x1b[32m", "\x1b[33m", "\x1b[31m"};

static int pnl_command_char(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80)
        return c;
    return 0;
}

static void pnl_command_of(const char *label, char *out)
{
    size_t len = 0;
    int dash = 0;

    for (; *label != '\0' && len < PNL_COMMAND_MAX - 2; label++) {
        int c = pnl_command_char((unsigned char)*label);

        if (c == 0) {
            dash = 1;
            continue;
        }
        if (dash && len > 0)
            out[len++] = '-';
        out[len++] = (char)c;
        dash = 0;
    }
    out[len] = '\0';
}

static void pnl_json_string(const char *text)
{
    putchar('"');
    for (; *text != '\0'; text++) {
        unsigned char c = (unsigned char)*text;

        if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c < 0x20)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}

static void pnl_cli_line(pnl_app_t *app, int mark, const char *text)
{
    if (mark > app->cli_worst)
        app->cli_worst = mark;
    if (app->cli_color)
        printf("[%s%s\x1b[0m] %s\n", pnl_mark_colors[mark], pnl_mark_names[mark], text);
    else
        printf("[%s] %s\n", pnl_mark_names[mark], text);
}

static void pnl_json_fields(int mark, const char *text)
{
    printf("{\"mark\":\"%s\",\"text\":", pnl_mark_json[mark]);
    pnl_json_string(text);
}

static void pnl_json_entry(int mark, const char *text)
{
    pnl_json_fields(mark, text);
    putchar('}');
}

int pnl_cli_result(pnl_app_t *app, int mark, const char *text)
{
    if (!app->cli_json) {
        pnl_cli_line(app, mark, text);
        return PNL_OK;
    }
    if (mark > app->cli_worst)
        app->cli_worst = mark;
    pnl_json_entry(mark, text);
    putchar('\n');
    return PNL_OK;
}

void pnl_cli_progress(const char *step, unsigned long long done, unsigned long long total)
{
    if (!pnl_term_stderr_is_tty())
        return;
    if (total > 0)
        fprintf(stderr, "\r\x1b[K%3d%%  %s", (int)(done >= total ? 100 : done * 100 / total), step);
    else
        fprintf(stderr, "\r\x1b[K%s", step);
    fflush(stderr);
}

void pnl_cli_log(const pnl_app_t *app, const char *line)
{
    if (app->cli_json)
        return;
    if (pnl_term_stderr_is_tty())
        fputs("\r\x1b[K", stderr);
    puts(line);
    fflush(stdout);
}

int pnl_cli_read_line(const char *prompt, char *text, size_t text_len)
{
    size_t len;
    int c;

    if (pnl_term_stdin_is_tty()) {
        fprintf(stderr, "%s", prompt);
        fflush(stderr);
    }
    fflush(stdout);
    if (fgets(text, (int)(text_len > 4096 ? 4096 : text_len), stdin) == NULL)
        return 0;
    len = strlen(text);
    if (len > 0 && text[len - 1] == '\n') {
        text[--len] = '\0';
        if (len > 0 && text[len - 1] == '\r')
            text[--len] = '\0';
        return 1;
    }
    while ((c = getchar()) != EOF && c != '\n')
        ;
    return 1;
}

static const char *pnl_cli_cell(const char *const *headers, size_t columns, const char *const *cells, size_t row,
                                size_t column)
{
    const char *text = row == 0 ? headers[column] : cells[(row - 1) * columns + column];

    return text ? text : "";
}

static void pnl_cli_table_json(const char *const *headers, size_t columns, const char *const *cells, size_t rows)
{
    size_t r;
    size_t c;

    putchar('[');
    for (r = 1; r <= rows; r++) {
        printf(r > 1 ? ",{" : "{");
        for (c = 0; c < columns; c++) {
            if (c > 0)
                putchar(',');
            pnl_json_string(pnl_cli_cell(headers, columns, cells, 0, c));
            putchar(':');
            pnl_json_string(pnl_cli_cell(headers, columns, cells, r, c));
        }
        putchar('}');
    }
    printf("]\n");
}

static int pnl_cli_column_width(const char *const *headers, size_t columns, const char *const *cells, size_t rows,
                                size_t column)
{
    int width = 0;
    size_t r;

    for (r = 0; r <= rows; r++) {
        int w = pnl_text_width(pnl_cli_cell(headers, columns, cells, r, column));

        width = w > width ? w : width;
    }
    return width;
}

int pnl_cli_table(pnl_app_t *app, const char *const *headers, const int *column_flags, size_t columns,
                  const char *const *cells, size_t rows)
{
    int *widths;
    size_t r;
    size_t c;

    if (app->cli_json) {
        pnl_cli_table_json(headers, columns, cells, rows);
        return PNL_OK;
    }
    widths = pnl_mem_alloc(&app->mem, columns * sizeof(*widths));
    if (widths == NULL)
        return PNL_ERR_NOMEM;
    for (c = 0; c < columns; c++)
        widths[c] = pnl_cli_column_width(headers, columns, cells, rows, c);
    for (r = 0; r <= rows; r++) {
        for (c = 0; c < columns; c++) {
            const char *text = pnl_cli_cell(headers, columns, cells, r, c);
            int pad = widths[c] - pnl_text_width(text);
            int right = column_flags != NULL && (column_flags[c] & PNL_COLUMN_RIGHT);

            printf("%*s%s%*s", right ? pad : 0, "", text, c + 1 < columns ? (right ? 0 : pad) + 2 : 0, "");
        }
        putchar('\n');
    }
    fflush(stdout);
    pnl_mem_free(&app->mem, widths);
    return PNL_OK;
}

static void pnl_cli_status_line(pnl_app_t *app, const struct pnl_line *line, int label_width)
{
    char percent[16] = "";
    char text[640];

    if (line->meter >= 0.0)
        snprintf(percent, sizeof(percent), "%s%d%%", line->text[0] != '\0' ? "  " : "",
                 (int)(line->meter * 100 + 0.5));
    if (line->label != NULL)
        snprintf(text, sizeof(text), "%s%*s  %s%s", line->label,
                 label_width > pnl_text_width(line->label) ? label_width - pnl_text_width(line->label) : 0, "",
                 line->text, percent);
    else
        snprintf(text, sizeof(text), "%s%s", line->text, percent);
    pnl_cli_line(app, line->mark, text);
}

static void pnl_cli_status(pnl_app_t *app)
{
    size_t i;

    pnl_menu_refresh(app);
    if (app->cli_json)
        printf("{\"status\":[");
    for (i = 0; i < app->lines_len; i++) {
        const struct pnl_line *line = &app->lines[i];

        if (!app->cli_json) {
            pnl_cli_status_line(app, line, pnl_label_width(app));
            continue;
        }
        if (line->mark > app->cli_worst)
            app->cli_worst = line->mark;
        if (i > 0)
            putchar(',');
        printf("{\"mark\":\"%s\"", pnl_mark_json[line->mark]);
        if (line->label != NULL) {
            printf(",\"label\":");
            pnl_json_string(line->label);
        }
        printf(",\"text\":");
        pnl_json_string(line->text);
        if (line->meter >= 0.0)
            printf(",\"meter\":%.3f", line->meter);
        putchar('}');
    }
    if (app->cli_json)
        printf("]}\n");
    pnl_menu_clear_lines(app);
}

static const char *pnl_program_name(const pnl_app_t *app, int argc, char **argv)
{
    const char *name;
    const char *slash;

    if (argc < 1 || argv == NULL || argv[0] == NULL || argv[0][0] == '\0')
        return app->title;
    name = argv[0];
    slash = (strrchr)(name, '/');
    if (slash == NULL)
        slash = (strrchr)(name, '\\');
    return slash ? slash + 1 : name;
}

static void pnl_cli_help(const pnl_app_t *app, const char *program, FILE *out)
{
    char command[PNL_COMMAND_MAX];
    size_t i;

    fprintf(out, "usage: %s [command] [--json] [--yes]\n\n%s\n\ncommands:\n", program, app->title);
    fprintf(out, "  %-20s %s\n", "status", "show the status lines");
    for (i = 0; i < app->actions_len; i++) {
        pnl_command_of(app->actions[i].label, command);
        fprintf(out, "  %-20s %s\n", command, app->actions[i].label);
    }
    fprintf(out, "\nwithout a command it opens the interactive panel, or prints the status when\n"
                 "the output is not a terminal\n");
}

static int pnl_find_action(const pnl_app_t *app, const char *name)
{
    char command[PNL_COMMAND_MAX];
    size_t i;

    for (i = 0; i < app->actions_len; i++) {
        pnl_command_of(app->actions[i].label, command);
        if (strcmp(command, name) == 0)
            return (int)i;
    }
    return -1;
}

struct pnl_args {
    const char *command;
    int help;
    int json;
    int yes;
    const char *bad;
};

static void pnl_parse_args(int argc, char **argv, struct pnl_args *args)
{
    int i;

    memset(args, 0, sizeof(*args));
    for (i = 1; i < argc && argv != NULL; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
            args->help = 1;
        else if (strcmp(arg, "--json") == 0)
            args->json = 1;
        else if (strcmp(arg, "--yes") == 0 || strcmp(arg, "-y") == 0)
            args->yes = 1;
        else if (arg[0] == '-' || args->command != NULL)
            args->bad = args->bad ? args->bad : arg;
        else
            args->command = arg;
    }
}

static int pnl_exit_for(const pnl_app_t *app)
{
    if (app->cli_needs_tty)
        return PNL_EXIT_FAIL;
    return app->cli_worst == PNL_MARK_FAIL ? PNL_EXIT_FAIL : PNL_EXIT_OK;
}

static int pnl_cli_action(pnl_app_t *app, int index)
{
    app->actions[index].fn(app->actions[index].ctx, app);
    fflush(stdout);
    if (app->cli_needs_tty)
        fprintf(stderr, "%s: this action needs an interactive terminal\n", app->actions[index].label);
    return pnl_exit_for(app);
}

static int pnl_cli_dispatch(pnl_app_t *app, const struct pnl_args *args, const char *program)
{
    int index;

    if (args->help) {
        pnl_cli_help(app, program, stdout);
        return PNL_EXIT_OK;
    }
    if (args->bad != NULL) {
        fprintf(stderr, "%s: unexpected argument '%s'\n\n", program, args->bad);
        pnl_cli_help(app, program, stderr);
        return PNL_EXIT_USAGE;
    }
    if (args->command == NULL || strcmp(args->command, "status") == 0) {
        pnl_cli_status(app);
        return pnl_exit_for(app);
    }
    index = pnl_find_action(app, args->command);
    if (index >= 0)
        return pnl_cli_action(app, index);
    fprintf(stderr, "%s: unknown command '%s'\n\n", program, args->command);
    pnl_cli_help(app, program, stderr);
    return PNL_EXIT_USAGE;
}

static int pnl_wants_color(void)
{
    return pnl_color_detect() != PNL_DEPTH_NONE && pnl_term_stdout_is_tty();
}

int pnl_app_run(pnl_app_t *app, int argc, char **argv, int *out_exit_code)
{
    struct pnl_args args;
    int exit_code;

    if (app == NULL || argc < 0 || (argc > 0 && argv == NULL))
        return PNL_ERR_ARG;
    pnl_parse_args(argc, argv, &args);
    if (args.command == NULL && !args.help && args.bad == NULL && !args.json && pnl_term_available()) {
        int status = pnl_run_interactive(app);

        if (out_exit_code != NULL)
            *out_exit_code = status == PNL_OK ? PNL_EXIT_OK : PNL_EXIT_FAIL;
        return status;
    }
    app->cli = 1;
    app->cli_json = args.json;
    app->cli_yes = args.yes;
    app->cli_color = !args.json && pnl_wants_color();
    app->cli_worst = PNL_MARK_OK;
    app->cli_needs_tty = 0;
    exit_code = pnl_cli_dispatch(app, &args, pnl_program_name(app, argc, argv));
    fflush(stdout);
    app->cli = 0;
    if (out_exit_code != NULL)
        *out_exit_code = exit_code;
    return PNL_OK;
}
