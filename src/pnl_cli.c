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

static void pnl_json_entry(int mark, const char *text)
{
    printf("{\"mark\":\"%s\",\"text\":", pnl_mark_json[mark]);
    pnl_json_string(text);
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

static void pnl_cli_status(pnl_app_t *app)
{
    size_t i;

    pnl_menu_refresh(app);
    if (app->cli_json)
        printf("{\"status\":[");
    for (i = 0; i < app->lines_len; i++) {
        if (!app->cli_json) {
            pnl_cli_line(app, app->lines[i].mark, app->lines[i].text);
            continue;
        }
        if (app->lines[i].mark > app->cli_worst)
            app->cli_worst = app->lines[i].mark;
        if (i > 0)
            putchar(',');
        pnl_json_entry(app->lines[i].mark, app->lines[i].text);
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

    fprintf(out, "usage: %s [command] [--json]\n\n%s\n\ncommands:\n", program, app->title);
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
    const char *no_color = getenv("NO_COLOR");

    if (no_color != NULL && no_color[0] != '\0')
        return 0;
    return pnl_term_stdout_is_tty();
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
