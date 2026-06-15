#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_KEYMAP_LINE 128
#define PNL_KEYMAP_PATH 1024

struct pnl_key_name {
    const char *name;
    int key;
};

static const struct pnl_key_name pnl_key_names[] = {
    {"up", PNL_KEY_UP},           {"down", PNL_KEY_DOWN},         {"left", PNL_KEY_LEFT},
    {"right", PNL_KEY_RIGHT},     {"enter", PNL_KEY_ENTER},       {"esc", PNL_KEY_ESCAPE},
    {"escape", PNL_KEY_ESCAPE},   {"pgup", PNL_KEY_PAGE_UP},      {"pageup", PNL_KEY_PAGE_UP},
    {"pgdn", PNL_KEY_PAGE_DOWN},  {"pagedown", PNL_KEY_PAGE_DOWN}, {"home", PNL_KEY_HOME},
    {"end", PNL_KEY_END},         {"backspace", PNL_KEY_BACKSPACE}, {"delete", PNL_KEY_DELETE},
    {"tab", PNL_KEY_TAB},         {"backtab", PNL_KEY_BACKTAB},   {"space", ' '},
    {"f12", PNL_KEY_INSPECT},     {"none", PNL_KEY_NONE},
};

int pnl_key_parse(const char *name, int *out_key)
{
    size_t i;

    for (i = 0; i < sizeof(pnl_key_names) / sizeof(pnl_key_names[0]); i++) {
        if (strcmp(pnl_key_names[i].name, name) == 0) {
            *out_key = pnl_key_names[i].key;
            return 1;
        }
    }
    if (strncmp(name, "ctrl-", 5) == 0 && name[5] >= 'a' && name[5] <= 'z' && name[6] == '\0') {
        *out_key = name[5] - 'a' + 1;
        return 1;
    }
    if ((unsigned char)name[0] > 0x20 && (unsigned char)name[0] < 0x7f && name[1] == '\0') {
        *out_key = (unsigned char)name[0];
        return 1;
    }
    return 0;
}

void pnl_key_label(int key, char *out, size_t out_len)
{
    size_t i;

    for (i = 0; i < sizeof(pnl_key_names) / sizeof(pnl_key_names[0]); i++) {
        if (pnl_key_names[i].key == key) {
            snprintf(out, out_len, "%s", pnl_key_names[i].name);
            return;
        }
    }
    if (key >= 1 && key <= 26)
        snprintf(out, out_len, "ctrl-%c", 'a' + key - 1);
    else if (key >= 0x110000)
        snprintf(out, out_len, "0x%x", (unsigned int)key);
    else if (key >= 0x80)
        out[pnl_utf8_put((uint32_t)key, out)] = '\0';
    else
        snprintf(out, out_len, "%c", key);
}

int pnl_app_bind(pnl_app_t *app, const char *key, const char *to)
{
    int from_key;
    int to_key;
    size_t i;

    if (app == NULL || key == NULL || to == NULL || !pnl_key_parse(key, &from_key) || !pnl_key_parse(to, &to_key) ||
        from_key == PNL_KEY_NONE || from_key == PNL_KEY_CTRL_C)
        return PNL_ERR_ARG;
    for (i = 0; i < app->bindings_len && app->bindings[i].from != from_key; i++)
        ;
    if (i == PNL_BINDINGS)
        return PNL_ERR_NOMEM;
    app->bindings[i].from = from_key;
    app->bindings[i].to = to_key;
    if (i == app->bindings_len)
        app->bindings_len++;
    return PNL_OK;
}

static int pnl_keymap_typing(const pnl_app_t *app)
{
    const struct pnl_view *top = app->views_len > 0 ? &app->views[app->views_len - 1] : NULL;

    return app->widget_typing || (top != NULL && top->typing_ref != NULL && *top->typing_ref);
}

int pnl_keymap_apply(const pnl_app_t *app, int key)
{
    size_t i;

    if (key >= 0x20 && key < 0x110000 && pnl_keymap_typing(app))
        return key;
    for (i = 0; i < app->bindings_len; i++) {
        if (app->bindings[i].from == key)
            return app->bindings[i].to;
    }
    return key;
}

static char *pnl_keymap_trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t')
        text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static void pnl_keymap_line(pnl_app_t *app, char *line)
{
    char *equals = (strchr)(line, '=');

    line = pnl_keymap_trim(line);
    if (line[0] == '#' || equals == NULL || equals == line)
        return;
    *equals = '\0';
    pnl_app_bind(app, pnl_keymap_trim(line), pnl_keymap_trim(equals + 1));
}

static void pnl_keymap_file(pnl_app_t *app, const char *path)
{
    char line[PNL_KEYMAP_LINE];
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return;
    while (fgets(line, sizeof(line), file) != NULL)
        pnl_keymap_line(app, line);
    fclose(file);
}

static int pnl_keymap_dir(char *out, size_t out_len)
{
#if defined(_WIN32)
    const char *base = getenv("APPDATA");

    if (base == NULL || base[0] == '\0')
        return 0;
    snprintf(out, out_len, "%s", base);
#else
    const char *base = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (base != NULL && base[0] == '/')
        snprintf(out, out_len, "%s", base);
    else if (home != NULL && home[0] != '\0')
        snprintf(out, out_len, "%s/.config", home);
    else
        return 0;
#endif
    return 1;
}

void pnl_keymap_load(pnl_app_t *app)
{
    char dir[PNL_KEYMAP_PATH];
    char name[128];
    char path[PNL_KEYMAP_PATH + 160];

    if (!pnl_keymap_dir(dir, sizeof(dir)))
        return;
    pnl_tool_name(name, sizeof(name), app->title);
    snprintf(path, sizeof(path), "%s/tuikit/keys", dir);
    pnl_keymap_file(app, path);
    snprintf(path, sizeof(path), "%s/%s/keys", dir, name);
    pnl_keymap_file(app, path);
}
