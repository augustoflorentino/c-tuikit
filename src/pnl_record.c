#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PNL_RECORD_START "\x1b[?25l\x1b[?7l"

struct pnl_recorder {
    FILE *file;
    long long start_ms;
    int width;
    int height;
};

static struct pnl_recorder pnl_recorder;

static void pnl_record_text(FILE *file, const char *text, size_t text_len)
{
    size_t i;

    fputc('"', file);
    for (i = 0; i < text_len; i++) {
        unsigned char c = (unsigned char)text[i];

        if (c == '"' || c == '\\')
            fprintf(file, "\\%c", c);
        else if (c < 0x20 || c == 0x7f)
            fprintf(file, "\\u%04x", c);
        else
            fputc(c, file);
    }
    fputc('"', file);
}

static void pnl_record_event(char kind, const char *data, size_t data_len)
{
    long long elapsed = pnl_term_now_ms() - pnl_recorder.start_ms;

    fprintf(pnl_recorder.file, "[%lld.%03lld, \"%c\", ", elapsed / 1000, elapsed % 1000, kind);
    pnl_record_text(pnl_recorder.file, data, data_len);
    fputs("]\n", pnl_recorder.file);
}

void pnl_record_open(pnl_app_t *app)
{
    const char *path = app->record_path != NULL ? app->record_path : getenv("TUIKIT_RECORD");

    if (path == NULL || path[0] == '\0' || pnl_recorder.file != NULL)
        return;
    pnl_recorder.file = fopen(path, "wb");
    if (pnl_recorder.file == NULL)
        return;
    pnl_term_size(&pnl_recorder.width, &pnl_recorder.height);
    pnl_recorder.start_ms = pnl_term_now_ms();
    fprintf(pnl_recorder.file, "{\"version\": 2, \"width\": %d, \"height\": %d", pnl_recorder.width,
            pnl_recorder.height);
    if (!pnl_virtual_active())
        fprintf(pnl_recorder.file, ", \"timestamp\": %lld", (long long)time(NULL));
    fputs(", \"title\": ", pnl_recorder.file);
    pnl_record_text(pnl_recorder.file, app->title, strlen(app->title));
    fputs("}\n", pnl_recorder.file);
    pnl_record_event('o', PNL_RECORD_START, sizeof(PNL_RECORD_START) - 1);
}

void pnl_record_output(const char *data, size_t data_len, int width, int height)
{
    if (pnl_recorder.file == NULL)
        return;
    if (width != pnl_recorder.width || height != pnl_recorder.height) {
        char size[32];
        int len = snprintf(size, sizeof(size), "%dx%d", width, height);

        pnl_recorder.width = width;
        pnl_recorder.height = height;
        pnl_record_event('r', size, (size_t)len);
    }
    pnl_record_event('o', data, data_len);
}

void pnl_record_close(void)
{
    if (pnl_recorder.file == NULL)
        return;
    fclose(pnl_recorder.file);
    pnl_recorder.file = NULL;
}

int pnl_app_record(pnl_app_t *app, const char *path)
{
    char *copy = NULL;

    if (app == NULL || (path != NULL && path[0] == '\0'))
        return PNL_ERR_ARG;
    if (path != NULL && (copy = pnl_mem_copy(&app->mem, path)) == NULL)
        return PNL_ERR_NOMEM;
    pnl_mem_free(&app->mem, app->record_path);
    app->record_path = copy;
    return PNL_OK;
}
