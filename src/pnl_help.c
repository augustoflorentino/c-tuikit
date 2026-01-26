#include "pnl_internal.h"

#include <string.h>

#define PNL_HELP_MAX 4096
#define PNL_HELP_KEY_WIDTH 14

static const char pnl_menu_hints[] = "up/down: move  enter: run the selected action  1-9: run that action  "
                                     "letter: jump to the next action starting with it  q: quit";
static const char pnl_global_hints[] = "?: this help  :: run any action by name  ctrl-c: quit  "
                                       "ctrl-z: suspend";

struct pnl_text {
    char data[PNL_HELP_MAX];
    size_t len;
};

static void pnl_text_add(struct pnl_text *text, const char *data, size_t len)
{
    if (len > sizeof(text->data) - 1 - text->len)
        len = sizeof(text->data) - 1 - text->len;
    memcpy(text->data + text->len, data, len);
    text->len += len;
    text->data[text->len] = '\0';
}

static void pnl_text_pad(struct pnl_text *text, size_t width)
{
    while (width-- > 0)
        pnl_text_add(text, " ", 1);
}

static void pnl_help_hint(struct pnl_text *text, const char *hint, size_t len)
{
    size_t key = 1;

    while (key + 1 < len && !(hint[key] == ':' && hint[key + 1] == ' '))
        key++;
    if (key + 1 >= len)
        return;
    pnl_text_add(text, "  ", 2);
    pnl_text_add(text, hint, key);
    pnl_text_pad(text, key < PNL_HELP_KEY_WIDTH ? PNL_HELP_KEY_WIDTH - key : 1);
    pnl_text_add(text, hint + key + 2, len - key - 2);
    pnl_text_add(text, "\n", 1);
}

static void pnl_help_section(struct pnl_text *text, const char *title, const char *hints)
{
    const char *pos = hints;

    pnl_text_add(text, title, strlen(title));
    pnl_text_add(text, "\n\n", 2);
    while (*pos != '\0') {
        const char *end = (strstr)(pos, "  ");
        size_t len = end ? (size_t)(end - pos) : strlen(pos);

        if (len > 0)
            pnl_help_hint(text, pos, len);
        pos += len;
        while (*pos == ' ')
            pos++;
    }
    pnl_text_add(text, "\n", 1);
}

void pnl_help(pnl_app_t *app)
{
    struct pnl_text text;
    const char *hints = pnl_view_hints(app);

    text.len = 0;
    text.data[0] = '\0';
    pnl_help_section(&text, "this screen", hints ? hints : pnl_menu_hints);
    pnl_help_section(&text, "everywhere", pnl_global_hints);
    pnl_pager(app, "help", text.data, text.len);
}

void pnl_palette(pnl_app_t *app)
{
    const char **labels;
    size_t index = 0;
    size_t i;
    int ok = 0;

    if (app->actions_len == 0)
        return;
    labels = pnl_mem_alloc(&app->mem, app->actions_len * sizeof(*labels));
    if (labels == NULL)
        return;
    for (i = 0; i < app->actions_len; i++)
        labels[i] = app->actions[i].label;
    if (pnl_pick(app, "run", labels, app->actions_len, &index, &ok) == PNL_OK && ok && !app->quit)
        app->actions[index].fn(app->actions[index].ctx, app);
    pnl_mem_free(&app->mem, labels);
}
