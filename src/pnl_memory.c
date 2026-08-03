#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define pnl_mkdir(path) _mkdir(path)
#define PNL_SEPARATOR '\\'
#else
#include <sys/stat.h>
#define pnl_mkdir(path) mkdir(path, 0700)
#define PNL_SEPARATOR '/'
#endif

#define PNL_MEMORY_FILE_MAX (1024 * 1024)
#define PNL_MEMORY_KEY_MAX 512
#define PNL_MEMORY_PATH_MAX 1024

static size_t pnl_memory_find(const pnl_app_t *app, const char *key)
{
    size_t i;

    for (i = 0; i < app->memos_len; i++) {
        if (strcmp(app->memos[i].key, key) == 0)
            return i;
    }
    return i;
}

static int pnl_memory_put(pnl_app_t *app, const char *key, const char *value)
{
    size_t i = pnl_memory_find(app, key);
    char *copy = pnl_mem_copy(&app->mem, value);

    if (copy == NULL)
        return PNL_ERR_NOMEM;
    if (i < app->memos_len) {
        pnl_mem_free(&app->mem, app->memos[i].value);
        app->memos[i].value = copy;
        return PNL_OK;
    }
    if (pnl_mem_grow(&app->mem, (void **)&app->memos, &app->memos_cap, app->memos_len + 1,
                     sizeof(*app->memos)) != PNL_OK ||
        (app->memos[i].key = pnl_mem_copy(&app->mem, key)) == NULL) {
        pnl_mem_free(&app->mem, copy);
        return PNL_ERR_NOMEM;
    }
    app->memos[i].value = copy;
    app->memos_len++;
    return PNL_OK;
}

static void pnl_memory_key(char *key, const char *kind, const char *name)
{
    snprintf(key, PNL_MEMORY_KEY_MAX, "%s:%s", kind, name);
}

const char *pnl_memory_get(const pnl_app_t *app, const char *kind, const char *name)
{
    char key[PNL_MEMORY_KEY_MAX];
    size_t i;

    if (app->memory_path == NULL)
        return NULL;
    pnl_memory_key(key, kind, name);
    i = pnl_memory_find(app, key);
    return i < app->memos_len ? app->memos[i].value : NULL;
}

void pnl_memory_set(pnl_app_t *app, const char *kind, const char *name, const char *value)
{
    char key[PNL_MEMORY_KEY_MAX];
    const char *old = pnl_memory_get(app, kind, name);

    if (app->memory_path == NULL || (old != NULL && strcmp(old, value) == 0))
        return;
    pnl_memory_key(key, kind, name);
    if (pnl_memory_put(app, key, value) == PNL_OK)
        app->memos_dirty = 1;
}

static void pnl_memory_unescape(char *text)
{
    char *out = text;

    for (; *text != '\0'; text++) {
        if (*text == '\\' && text[1] != '\0') {
            text++;
            *out++ = *text == 'n' ? '\n' : *text == 't' ? '\t' : *text;
        } else {
            *out++ = *text;
        }
    }
    *out = '\0';
}

void pnl_memory_parse(pnl_app_t *app, char *data)
{
    char *line = data;

    while (line != NULL && *line != '\0') {
        char *next = (strchr)(line, '\n');
        char *tab;

        if (next != NULL)
            *next++ = '\0';
        tab = (strchr)(line, '\t');
        if (tab != NULL) {
            *tab = '\0';
            pnl_memory_unescape(line);
            pnl_memory_unescape(tab + 1);
            pnl_memory_put(app, line, tab + 1);
        }
        line = next;
    }
}

void pnl_memory_load(pnl_app_t *app)
{
    FILE *file;
    char *data;
    size_t len;

    if (app->memory_path == NULL || app->memos_len > 0)
        return;
    file = fopen(app->memory_path, "rb");
    if (file == NULL)
        return;
    data = pnl_mem_alloc(&app->mem, PNL_MEMORY_FILE_MAX + 1);
    if (data != NULL) {
        len = fread(data, 1, PNL_MEMORY_FILE_MAX, file);
        data[len] = '\0';
        pnl_memory_parse(app, data);
    }
    pnl_mem_free(&app->mem, data);
    fclose(file);
}

static void pnl_memory_write(FILE *file, const char *text)
{
    for (; *text != '\0'; text++) {
        if (*text == '\\' || *text == '\n' || *text == '\t')
            fputc('\\', file);
        fputc(*text == '\n' ? 'n' : *text == '\t' ? 't' : *text, file);
    }
}

static void pnl_memory_mkdirs(const char *path)
{
    char dir[PNL_MEMORY_PATH_MAX];
    size_t i;

    snprintf(dir, sizeof(dir), "%s", path);
    for (i = 1; dir[i] != '\0'; i++) {
        if (dir[i] != '/' && dir[i] != PNL_SEPARATOR)
            continue;
        dir[i] = '\0';
        pnl_mkdir(dir);
        dir[i] = PNL_SEPARATOR;
    }
}

void pnl_memory_save(pnl_app_t *app)
{
    char temp[PNL_MEMORY_PATH_MAX];
    FILE *file;
    size_t i;
    int ok;

    if (app->memory_path == NULL || !app->memos_dirty)
        return;
    pnl_memory_mkdirs(app->memory_path);
    snprintf(temp, sizeof(temp), "%s.new", app->memory_path);
    file = fopen(temp, "wb");
    if (file == NULL)
        return;
    for (i = 0; i < app->memos_len; i++) {
        pnl_memory_write(file, app->memos[i].key);
        fputc('\t', file);
        pnl_memory_write(file, app->memos[i].value);
        fputc('\n', file);
    }
    ok = fclose(file) == 0;
#if defined(_WIN32)
    remove(app->memory_path);
#endif
    if (ok && rename(temp, app->memory_path) == 0)
        app->memos_dirty = 0;
    else
        remove(temp);
}

void pnl_memory_free(pnl_app_t *app)
{
    size_t i;

    for (i = 0; i < app->memos_len; i++) {
        pnl_mem_free(&app->mem, app->memos[i].key);
        pnl_mem_free(&app->mem, app->memos[i].value);
    }
    pnl_mem_free(&app->mem, app->memos);
    pnl_mem_free(&app->mem, app->memory_path);
    app->memos = NULL;
    app->memos_len = 0;
    app->memos_cap = 0;
    app->memory_path = NULL;
}

void pnl_tool_name(char *out, size_t out_len, const char *title)
{
    size_t i = 0;

    for (; *title != '\0' && i + 1 < out_len; title++) {
        char c = *title;

        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        out[i++] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ? c : '-';
    }
    out[i] = '\0';
}

static int pnl_memory_default(const pnl_app_t *app, char *path)
{
    char name[128];
    const char *base;
    const char *home;

    pnl_tool_name(name, sizeof(name), app->title);
#if defined(_WIN32)
    base = getenv("LOCALAPPDATA");
    home = NULL;
    if (base == NULL || base[0] == '\0')
        return 0;
    snprintf(path, PNL_MEMORY_PATH_MAX, "%s\\%s\\memory", base, name);
#else
    base = getenv("XDG_STATE_HOME");
    home = getenv("HOME");
    if (base != NULL && base[0] == '/')
        snprintf(path, PNL_MEMORY_PATH_MAX, "%s/%s/memory", base, name);
    else if (home != NULL && home[0] != '\0')
        snprintf(path, PNL_MEMORY_PATH_MAX, "%s/.local/state/%s/memory", home, name);
    else
        return 0;
#endif
    (void)home;
    return 1;
}

int pnl_app_set_memory(pnl_app_t *app, const char *path)
{
    char fallback[PNL_MEMORY_PATH_MAX];
    char *copy;

    if (app == NULL || (path != NULL && (path[0] == '\0' || strlen(path) + 5 >= PNL_MEMORY_PATH_MAX)))
        return PNL_ERR_ARG;
    pnl_memory_free(app);
    if (pnl_env_off("TUIKIT_MEMORY"))
        return PNL_OK;
    if (path == NULL && !pnl_memory_default(app, fallback))
        return PNL_OK;
    copy = pnl_mem_copy(&app->mem, path != NULL ? path : fallback);
    if (copy == NULL)
        return PNL_ERR_NOMEM;
    app->memory_path = copy;
    return PNL_OK;
}
