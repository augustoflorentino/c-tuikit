#include "pnl_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_VIRTUAL_KEYS 64
#define PNL_VIRTUAL_TIMEOUT_MS 30000
#define PNL_VIRTUAL_NAP_MS 1
#define PNL_VIRTUAL_BUSY_MS 5
#define PNL_TEST_FILE_MAX (4 * 1024 * 1024)
#define PNL_TEST_PATH_MAX 1024

struct pnl_test {
    pnl_app_t *app;
    struct pnl_lock lock;
    struct pnl_thread thread;
    int width;
    int height;
    int keys[PNL_VIRTUAL_KEYS];
    size_t keys_len;
    long long clock_ms;
    long long clock_seen;
    int woken;
    int idle;
    int finished;
    int status;
    char *text;
    size_t text_cap;
};

static pnl_test_t *pnl_test_now;

int pnl_virtual_active(void)
{
    return pnl_test_now != NULL;
}

int pnl_term_enter(struct pnl_term *term)
{
    if (pnl_test_now == NULL)
        return pnl_os_enter(term);
    term->active = 1;
    return PNL_OK;
}

void pnl_term_leave(struct pnl_term *term)
{
    if (pnl_test_now == NULL)
        pnl_os_leave(term);
    else
        term->active = 0;
}

void pnl_term_suspend(struct pnl_term *term)
{
    if (pnl_test_now == NULL)
        pnl_os_suspend(term);
}

int pnl_term_resized(void)
{
    return pnl_test_now == NULL ? pnl_os_resized() : 0;
}

void pnl_term_size(int *out_width, int *out_height)
{
    if (pnl_test_now == NULL) {
        pnl_os_size(out_width, out_height);
        return;
    }
    *out_width = pnl_test_now->width;
    *out_height = pnl_test_now->height;
}

int pnl_term_write(const char *data, size_t data_len)
{
    return pnl_test_now == NULL ? pnl_os_write(data, data_len) : PNL_OK;
}

long long pnl_term_now_ms(void)
{
    long long now;

    if (pnl_test_now == NULL)
        return pnl_os_now_ms();
    pnl_lock(&pnl_test_now->lock);
    now = pnl_test_now->clock_ms;
    pnl_unlock(&pnl_test_now->lock);
    return now;
}

void pnl_term_wake(struct pnl_term *term)
{
    if (pnl_test_now == NULL) {
        pnl_os_wake(term);
        return;
    }
    pnl_lock(&pnl_test_now->lock);
    pnl_test_now->woken = 1;
    pnl_unlock(&pnl_test_now->lock);
}

int pnl_term_read_key(struct pnl_term *term, int timeout_ms, int *out_key)
{
    pnl_test_t *test = pnl_test_now;

    if (test == NULL)
        return pnl_os_read_key(term, timeout_ms, out_key);
    pnl_lock(&test->lock);
    *out_key = PNL_KEY_NONE;
    if (test->keys_len > 0) {
        *out_key = test->keys[0];
        memmove(test->keys, test->keys + 1, --test->keys_len * sizeof(*test->keys));
    }
    pnl_unlock(&test->lock);
    return PNL_OK;
}

static void pnl_virtual_snapshot(pnl_test_t *test, const struct pnl_screen *screen)
{
    const struct pnl_cell *cells = screen->front_valid ? screen->front : screen->back;
    size_t len = 0;
    int x;
    int y;

    for (y = 0; cells != NULL && y < screen->height && y < test->height; y++) {
        size_t row = len;

        for (x = 0; x < screen->width && x < test->width; x++) {
            uint32_t cp = cells[(size_t)y * (size_t)screen->width + (size_t)x].cp;

            if (cp != 0)
                len += pnl_utf8_put(cp, test->text + len);
            if (cp != 0 && cp != ' ')
                row = len;
        }
        len = row;
        test->text[len++] = '\n';
    }
    test->text[len] = '\0';
}

static int pnl_virtual_ready(pnl_test_t *test)
{
    if (test->keys_len > 0)
        return 1;
    if (test->woken) {
        test->woken = 0;
        return 0;
    }
    if (test->clock_ms != test->clock_seen) {
        test->clock_seen = test->clock_ms;
        return 0;
    }
    return -1;
}

int pnl_term_wait(struct pnl_term *term, const struct pnl_watch *watches, size_t watches_len, int timeout_ms,
                  unsigned char *out_ready)
{
    pnl_test_t *test = pnl_test_now;
    pnl_app_t *app;

    if (test == NULL)
        return pnl_os_wait(term, watches, watches_len, timeout_ms, out_ready);
    app = (pnl_app_t *)(void *)((char *)term - offsetof(pnl_app_t, term));
    for (;;) {
        int ready;

        pnl_lock(&test->lock);
        ready = pnl_virtual_ready(test);
        if (ready == 0)
            term->woken = 1;
        if (ready < 0 && app->tasks_running == 0 && !test->idle) {
            pnl_virtual_snapshot(test, &app->screen);
            test->idle = 1;
        }
        pnl_unlock(&test->lock);
        if (ready >= 0)
            return ready;
        if (timeout_ms == 0)
            return 0;
        if (app->tasks_running > 0) {
            pnl_nap_ms(PNL_VIRTUAL_BUSY_MS);
            return 0;
        }
        pnl_nap_ms(PNL_VIRTUAL_NAP_MS);
    }
}

static int pnl_test_settle(pnl_test_t *test)
{
    long long waited = 0;

    for (;;) {
        int done;

        pnl_lock(&test->lock);
        done = test->finished || (test->idle && test->keys_len == 0);
        pnl_unlock(&test->lock);
        if (done)
            return PNL_OK;
        if (waited >= PNL_VIRTUAL_TIMEOUT_MS)
            return PNL_ERR_IO;
        pnl_nap_ms(PNL_VIRTUAL_NAP_MS);
        waited += PNL_VIRTUAL_NAP_MS;
    }
}

static void pnl_test_main(void *arg)
{
    pnl_test_t *test = arg;
    int status = pnl_run_interactive(test->app);

    pnl_lock(&test->lock);
    test->status = status;
    test->finished = 1;
    pnl_virtual_snapshot(test, &test->app->screen);
    pnl_unlock(&test->lock);
}

static int pnl_test_push(pnl_test_t *test, int key)
{
    pnl_lock(&test->lock);
    if (test->finished) {
        pnl_unlock(&test->lock);
        return PNL_OK;
    }
    test->keys[test->keys_len++] = key;
    test->idle = 0;
    pnl_unlock(&test->lock);
    return pnl_test_settle(test);
}

static void pnl_test_free(pnl_test_t *test)
{
    pnl_app_t *app = test->app;

    pnl_lock_destroy(&test->lock);
    pnl_mem_free(&app->mem, test->text);
    pnl_mem_free(&app->mem, test);
}

int pnl_test_create(pnl_app_t *app, int width, int height, pnl_test_t **out_test)
{
    pnl_test_t *test;
    int status;

    if (out_test != NULL)
        *out_test = NULL;
    if (app == NULL || out_test == NULL || width < 10 || height < 4 || width > 1000 || height > 1000 ||
        app->running || app->cli || pnl_test_now != NULL)
        return PNL_ERR_ARG;
    test = pnl_mem_alloc(&app->mem, sizeof(*test));
    if (test == NULL)
        return PNL_ERR_NOMEM;
    memset(test, 0, sizeof(*test));
    test->app = app;
    test->width = width;
    test->height = height;
    test->text_cap = (size_t)width * (size_t)height * 4 + (size_t)height + 1;
    test->text = pnl_mem_alloc(&app->mem, test->text_cap);
    status = test->text == NULL ? PNL_ERR_NOMEM : pnl_lock_init(&test->lock);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, test->text);
        pnl_mem_free(&app->mem, test);
        return status;
    }
    test->text[0] = '\0';
    pnl_test_now = test;
    status = pnl_thread_start(&test->thread, pnl_test_main, test);
    if (status != PNL_OK) {
        pnl_test_now = NULL;
        pnl_test_free(test);
        return status;
    }
    status = pnl_test_settle(test);
    if (status != PNL_OK) {
        pnl_test_destroy(test);
        return status;
    }
    *out_test = test;
    return PNL_OK;
}

void pnl_test_destroy(pnl_test_t *test)
{
    if (test == NULL)
        return;
    pnl_lock(&test->lock);
    test->keys_len = 0;
    if (!test->finished)
        test->keys[test->keys_len++] = PNL_KEY_CTRL_C;
    pnl_unlock(&test->lock);
    pnl_thread_join(&test->thread);
    pnl_test_now = NULL;
    pnl_test_free(test);
}

static const char *pnl_test_token(const char *at, char *out, size_t out_len)
{
    size_t len = 0;

    while (*at == ' ')
        at++;
    if (*at == '"') {
        for (at++; *at != '\0' && *at != '"' && len + 1 < out_len; at++)
            out[len++] = *at;
        out[len] = '\0';
        return *at == '"' ? at + 1 : at;
    }
    for (; *at != '\0' && *at != ' ' && len + 1 < out_len; at++)
        out[len++] = *at;
    out[len] = '\0';
    return at;
}

static int pnl_test_text(pnl_test_t *test, const char *text)
{
    int status = PNL_OK;

    while (*text != '\0' && status == PNL_OK) {
        int cp = (unsigned char)*text++;

        if (cp >= 0x80) {
            int extra = cp >= 0xf0 ? 3 : cp >= 0xe0 ? 2 : 1;

            cp &= extra == 3 ? 0x07 : extra == 2 ? 0x0f : 0x1f;
            while (extra-- > 0 && (*text & 0xc0) == 0x80)
                cp = (cp << 6) | (*text++ & 0x3f);
        }
        status = pnl_test_push(test, cp);
    }
    return status;
}

int pnl_test_keys(pnl_test_t *test, const char *keys)
{
    char token[256];
    int status = PNL_OK;

    if (test == NULL || keys == NULL)
        return PNL_ERR_ARG;
    while (*keys != '\0' && status == PNL_OK) {
        int quoted;
        int key;

        while (*keys == ' ')
            keys++;
        quoted = *keys == '"';
        keys = pnl_test_token(keys, token, sizeof(token));
        if (quoted)
            status = pnl_test_text(test, token);
        else if (token[0] == '\0')
            break;
        else if (!pnl_key_parse(token, &key) || key == PNL_KEY_NONE)
            status = PNL_ERR_ARG;
        else
            status = pnl_test_push(test, key);
    }
    return status;
}

int pnl_test_wait(pnl_test_t *test, int ms)
{
    if (test == NULL || ms < 0)
        return PNL_ERR_ARG;
    pnl_lock(&test->lock);
    test->clock_ms += ms;
    test->idle = test->finished;
    pnl_unlock(&test->lock);
    return pnl_test_settle(test);
}

const char *pnl_test_screen(const pnl_test_t *test)
{
    return test != NULL ? test->text : "";
}

static char *pnl_test_read(pnl_app_t *app, const char *path)
{
    FILE *file = fopen(path, "rb");
    char *data;
    size_t len;

    if (file == NULL)
        return NULL;
    data = pnl_mem_alloc(&app->mem, PNL_TEST_FILE_MAX + 1);
    if (data != NULL) {
        len = fread(data, 1, PNL_TEST_FILE_MAX, file);
        data[len] = '\0';
    }
    fclose(file);
    return data;
}

static int pnl_test_save(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    int ok;

    if (file == NULL)
        return PNL_ERR_IO;
    ok = fputs(text, file) >= 0;
    ok = fclose(file) == 0 && ok;
    return ok ? PNL_OK : PNL_ERR_IO;
}

int pnl_test_expect(pnl_test_t *test, const char *path)
{
    const char *update = getenv("TUIKIT_SNAPSHOTS");
    char fresh[PNL_TEST_PATH_MAX];
    char *saved;
    int same;

    if (test == NULL || path == NULL || strlen(path) + 5 > sizeof(fresh))
        return PNL_ERR_ARG;
    saved = pnl_test_read(test->app, path);
    if (saved == NULL || (update != NULL && strcmp(update, "update") == 0)) {
        pnl_mem_free(&test->app->mem, saved);
        return pnl_test_save(path, test->text);
    }
    same = strcmp(saved, test->text) == 0;
    pnl_mem_free(&test->app->mem, saved);
    snprintf(fresh, sizeof(fresh), "%s.new", path);
    if (same) {
        remove(fresh);
        return PNL_OK;
    }
    pnl_test_save(fresh, test->text);
    return PNL_ERR_MISMATCH;
}
