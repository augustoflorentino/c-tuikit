#include <c-tuikit/c-tuikit.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LINE_MAX_LEN 1024

struct follow {
    const char *path;
    FILE *file;
    pnl_log_t *lines;
    pnl_value_t *file_value;
    pnl_value_t *count;
    unsigned long read;
    unsigned long errors;
};

static int has_word(const char *line, const char *word)
{
    size_t len = strlen(word);

    for (; *line != '\0'; line++) {
        size_t i = 0;

        while (i < len && tolower((unsigned char)line[i]) == word[i])
            i++;
        if (i == len)
            return 1;
    }
    return 0;
}

static int line_level(const char *line)
{
    if (has_word(line, "error") || has_word(line, "fatal") || has_word(line, "panic"))
        return PNL_LEVEL_ERROR;
    if (has_word(line, "warn"))
        return PNL_LEVEL_WARN;
    if (has_word(line, "debug") || has_word(line, "trace"))
        return PNL_LEVEL_DEBUG;
    return PNL_LEVEL_INFO;
}

static void show_count(struct follow *follow)
{
    char text[64];

    snprintf(text, sizeof(text), "%lu, %lu with errors", follow->read, follow->errors);
    pnl_value_set(follow->count, follow->errors > 0 ? PNL_MARK_FAIL : PNL_MARK_OK, text);
}

static void read_new_lines(void *ctx, pnl_app_t *app)
{
    struct follow *follow = ctx;
    char line[LINE_MAX_LEN];
    unsigned long before = follow->read;

    (void)app;
    clearerr(follow->file);
    while (fgets(line, sizeof(line), follow->file) != NULL) {
        int level = line_level(line);

        line[strcspn(line, "\r\n")] = '\0';
        pnl_log_write(follow->lines, level, line);
        follow->read++;
        follow->errors += level == PNL_LEVEL_ERROR;
    }
    if (follow->read != before)
        show_count(follow);
}

static void open_lines(void *ctx, pnl_app_t *app)
{
    struct follow *follow = ctx;

    pnl_log_open(app, follow->lines, follow->path);
}

static void forget_errors(void *ctx, pnl_app_t *app)
{
    struct follow *follow = ctx;

    follow->errors = 0;
    show_count(follow);
    pnl_toast(app, PNL_MARK_OK, "error count reset");
}

static int setup(pnl_app_t *app, struct follow *follow)
{
    int status = pnl_value_create(app, "file", &follow->file_value);

    if (status == PNL_OK)
        status = pnl_value_set(follow->file_value, PNL_MARK_OK, follow->path);
    if (status == PNL_OK)
        status = pnl_value_create(app, "lines", &follow->count);
    if (status == PNL_OK)
        status = pnl_log_create(app, 20000, &follow->lines);
    if (status == PNL_OK)
        status = pnl_app_add_timer(app, 250, read_new_lines, follow, NULL);
    if (status == PNL_OK)
        status = pnl_app_set_memory(app, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Lines", open_lines, follow);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "Forget errors", forget_errors, follow);
    if (status == PNL_OK)
        read_new_lines(follow, app);
    return status;
}

int main(int argc, char **argv)
{
    struct follow follow;
    pnl_app_t *app = NULL;
    int exit_code = 1;
    int status;

    if (argc < 2 || argv[1][0] == '-') {
        fprintf(stderr, "usage: %s FILE [command]\n", argv[0]);
        return 2;
    }
    memset(&follow, 0, sizeof(follow));
    follow.path = argv[1];
    follow.file = fopen(follow.path, "r");
    if (follow.file == NULL) {
        perror(follow.path);
        return 1;
    }
    argv[1] = argv[0];
    status = pnl_app_create("tail", NULL, NULL, NULL, &app);
    if (status == PNL_OK)
        status = setup(app, &follow);
    if (status == PNL_OK)
        status = pnl_app_run(app, argc - 1, argv + 1, &exit_code);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    pnl_value_destroy(follow.file_value);
    pnl_value_destroy(follow.count);
    pnl_log_destroy(follow.lines);
    pnl_app_destroy(app);
    fclose(follow.file);
    return exit_code;
}
