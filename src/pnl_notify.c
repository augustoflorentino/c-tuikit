#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_NOTIFY_TEXT 512
#define PNL_NOTIFY_AFTER_MS 5000

static int pnl_notify_mode(void)
{
    const char *setting = getenv("TUIKIT_NOTIFY");

    if (setting == NULL || setting[0] == '\0')
        return 2;
    if (strcmp(setting, "off") == 0 || strcmp(setting, "0") == 0)
        return 0;
    return strcmp(setting, "bell") == 0 ? 1 : 2;
}

static void pnl_notify_clean(char *out, const char *title, const char *text)
{
    size_t i;

    snprintf(out, PNL_NOTIFY_TEXT, "%s: %s", title, text);
    for (i = 0; out[i] != '\0'; i++) {
        if ((unsigned char)out[i] < 0x20 || out[i] == 0x7f)
            out[i] = ' ';
    }
}

static void pnl_notify_send(pnl_app_t *app, const char *text)
{
    char message[PNL_NOTIFY_TEXT];
    char sequence[PNL_NOTIFY_TEXT + 16];
    int mode = pnl_notify_mode();
    int len;

    if (mode == 0)
        return;
    pnl_notify_clean(message, app->title, text);
    len = snprintf(sequence, sizeof(sequence), mode == 2 ? "\a\x1b]9;%s\a" : "\a", message);
    if (len <= 0)
        return;
    if (app->running) {
        pnl_term_write(sequence, strlen(sequence));
        return;
    }
    if (pnl_term_stderr_is_tty()) {
        fputs(sequence, stderr);
        fflush(stderr);
    }
}

int pnl_notify(pnl_app_t *app, const char *text)
{
    if (app == NULL || text == NULL || !(app->running || app->cli))
        return PNL_ERR_ARG;
    if (app->running && app->term.focus_known && app->term.focused)
        return PNL_OK;
    pnl_notify_send(app, text);
    return PNL_OK;
}

void pnl_task_notify(pnl_app_t *app, const char *title, long long started_ms, const char *outcome)
{
    char text[PNL_NOTIFY_TEXT];

    if (pnl_term_now_ms() - started_ms < PNL_NOTIFY_AFTER_MS)
        return;
    snprintf(text, sizeof(text), "%s - %s", title, outcome);
    pnl_notify(app, text);
}
