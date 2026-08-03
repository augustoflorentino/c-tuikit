#include "pnl_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void parse_lines(pnl_app_t *app, char *text, int theme)
{
    int light_set[PNL_ROLES] = {0};
    char *line = text;

    while (line != NULL) {
        char *next = (strchr)(line, '\n');

        if (next != NULL)
            *next++ = '\0';
        if (theme)
            pnl_theme_line(&app->theme, line, light_set);
        else
            pnl_keymap_line(app, line);
        line = next;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    pnl_app_t *app = NULL;
    char *text;

    if (size == 0 || pnl_app_create("fuzz", NULL, &app) != PNL_OK)
        return 0;
    text = malloc(size);
    if (text != NULL) {
        memcpy(text, data + 1, size - 1);
        text[size - 1] = '\0';
        if (data[0] % 3 == 0)
            pnl_memory_parse(app, text);
        else
            parse_lines(app, text, data[0] % 3 == 1);
    }
    free(text);
    pnl_app_destroy(app);
    return 0;
}
