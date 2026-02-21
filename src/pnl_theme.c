#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_THEME_LINE 256

struct pnl_builtin_theme {
    const char *name;
    struct pnl_theme theme;
};

static const struct pnl_builtin_theme pnl_builtin_themes[] = {
    {"default",
     {{0x5fafffUL, 0x5fd75fUL, 0xd7af00UL, 0xff5f5fUL, 0x8a8a8aUL, 0x87afd7UL, 0x3a3a3aUL},
      {0x005fd7UL, 0x008700UL, 0x875f00UL, 0xd70000UL, 0x6c6c6cUL, 0x005f87UL, 0xdadadaUL},
      {PNL_COLOR_CYAN, PNL_COLOR_GREEN, PNL_COLOR_YELLOW, PNL_COLOR_RED, PNL_COLOR_GRAY, PNL_COLOR_BLUE,
       PNL_COLOR_GRAY}}},
    {"mono",
     {{0xffffffUL, 0xd0d0d0UL, 0xd0d0d0UL, 0xffffffUL, 0x808080UL, 0xb0b0b0UL, 0x3a3a3aUL},
      {0x000000UL, 0x303030UL, 0x303030UL, 0x000000UL, 0x707070UL, 0x505050UL, 0xdadadaUL},
      {PNL_COLOR_WHITE, PNL_COLOR_DEFAULT, PNL_COLOR_DEFAULT, PNL_COLOR_WHITE, PNL_COLOR_GRAY, PNL_COLOR_DEFAULT,
       PNL_COLOR_GRAY}}},
    {"contrast",
     {{0x00d7ffUL, 0x00ff00UL, 0xffff00UL, 0xff3030UL, 0xbcbcbcUL, 0x87d7ffUL, 0x444444UL},
      {0x0000d7UL, 0x006400UL, 0x7a5c00UL, 0xb00000UL, 0x4e4e4eUL, 0x00008bUL, 0xc6c6c6UL},
      {PNL_COLOR_BRIGHT_CYAN, PNL_COLOR_BRIGHT_GREEN, PNL_COLOR_BRIGHT_YELLOW, PNL_COLOR_BRIGHT_RED,
       PNL_COLOR_WHITE, PNL_COLOR_BRIGHT_BLUE, PNL_COLOR_GRAY}}},
    {"warm",
     {{0xffaf5fUL, 0xafd75fUL, 0xffd75fUL, 0xff875fUL, 0x9e8e7eUL, 0xd7af87UL, 0x3a302aUL},
      {0xaf5f00UL, 0x5f8700UL, 0x875f00UL, 0xaf0000UL, 0x7a6a5aUL, 0x875f5fUL, 0xe4dccfUL},
      {PNL_COLOR_YELLOW, PNL_COLOR_GREEN, PNL_COLOR_YELLOW, PNL_COLOR_RED, PNL_COLOR_GRAY, PNL_COLOR_MAGENTA,
       PNL_COLOR_GRAY}}},
};

static const char *const pnl_role_names[PNL_ROLES] = {"accent", "ok", "warn", "fail", "muted", "info", "surface"};

#define PNL_BUILTIN_THEMES (sizeof(pnl_builtin_themes) / sizeof(pnl_builtin_themes[0]))

static int pnl_theme_builtin(struct pnl_theme *theme, const char *name)
{
    size_t i;

    for (i = 0; i < PNL_BUILTIN_THEMES; i++) {
        if (strcmp(pnl_builtin_themes[i].name, name) == 0) {
            *theme = pnl_builtin_themes[i].theme;
            return 1;
        }
    }
    return 0;
}

static char *pnl_trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t')
        text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
        *--end = '\0';
    return text;
}

static int pnl_theme_role(const char *name)
{
    int i;

    for (i = 0; i < PNL_ROLES; i++) {
        if (strcmp(pnl_role_names[i], name) == 0)
            return i;
    }
    return -1;
}

static int pnl_theme_color(const char *text, unsigned long *out_rgb)
{
    char *end;

    if (text[0] != '#' || strlen(text) != 7)
        return 0;
    *out_rgb = strtoul(text + 1, &end, 16);
    return *end == '\0';
}

static int pnl_theme_line(struct pnl_theme *theme, char *line, int *light_set)
{
    char *equal = (strchr)(line, '=');
    char *key;
    unsigned long rgb;
    int light = 0;
    int role;

    if (equal == NULL)
        return 0;
    *equal = '\0';
    key = pnl_trim(line);
    if (strncmp(key, "light.", 6) == 0) {
        light = 1;
        key += 6;
    }
    role = pnl_theme_role(key);
    if (role < 0 || !pnl_theme_color(pnl_trim(equal + 1), &rgb))
        return 0;
    if (light) {
        theme->light[role] = rgb;
        light_set[role] = 1;
    } else {
        theme->dark[role] = rgb;
    }
    return 1;
}

static int pnl_theme_file(struct pnl_theme *theme, const char *path)
{
    char line[PNL_THEME_LINE];
    int light_set[PNL_ROLES] = {0};
    FILE *file = fopen(path, "r");
    int status = PNL_OK;
    int i;

    if (file == NULL)
        return PNL_ERR_NOT_FOUND;
    pnl_theme_builtin(theme, "default");
    while (status == PNL_OK && fgets(line, sizeof(line), file) != NULL) {
        char *text = pnl_trim(line);

        if (text[0] != '\0' && text[0] != '#' && !pnl_theme_line(theme, text, light_set))
            status = PNL_ERR_ARG;
    }
    fclose(file);
    for (i = 0; i < PNL_ROLES; i++) {
        if (!light_set[i])
            theme->light[i] = theme->dark[i];
    }
    return status;
}

int pnl_theme_choose(struct pnl_theme *theme, const char *name)
{
    struct pnl_theme chosen;
    int status = PNL_OK;

    if (!pnl_theme_builtin(&chosen, name))
        status = (strchr)(name, '/') != NULL || (strchr)(name, '\\') != NULL || (strchr)(name, '.') != NULL
                     ? pnl_theme_file(&chosen, name)
                     : PNL_ERR_NOT_FOUND;
    if (status == PNL_OK)
        *theme = chosen;
    return status;
}

int pnl_app_set_theme(pnl_app_t *app, const char *theme)
{
    if (app == NULL || theme == NULL)
        return PNL_ERR_ARG;
    return pnl_theme_choose(&app->theme, theme);
}
