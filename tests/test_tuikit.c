#include "c-tuikit/c-tuikit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            exit(1);                                                    \
        }                                                               \
    } while (0)

typedef struct {
    int remaining;
    int live;
} failing_alloc_t;

static void *failing_alloc(void *ctx, size_t size)
{
    failing_alloc_t *fa = ctx;

    if (fa->remaining-- == 0)
        return NULL;
    fa->live++;
    return malloc(size);
}

static void failing_free(void *ctx, void *ptr)
{
    failing_alloc_t *fa = ctx;

    fa->live--;
    free(ptr);
}

static void noop_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void noop_status(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static int decode(const char *bytes, size_t *out_used)
{
    int key = -1;

    *out_used = pnl_key_decode((const unsigned char *)bytes, strlen(bytes), &key);
    return key;
}

static void test_keys(void)
{
    size_t used;
    int key = -1;

    CHECK(decode("\x1b[A", &used) == PNL_KEY_UP && used == 3);
    CHECK(decode("\x1b[B", &used) == PNL_KEY_DOWN && used == 3);
    CHECK(decode("\x1bOC", &used) == PNL_KEY_RIGHT && used == 3);
    CHECK(decode("\x1b[D", &used) == PNL_KEY_LEFT && used == 3);
    CHECK(decode("\x1b", &used) == PNL_KEY_ESCAPE && used == 1);
    CHECK(decode("\x1bx", &used) == PNL_KEY_ESCAPE && used == 1);
    CHECK(decode("\x1b[1;5A", &used) == PNL_KEY_NONE && used == 6);
    CHECK(decode("\x1b[5~", &used) == PNL_KEY_NONE && used == 4);
    CHECK(decode("\r", &used) == PNL_KEY_ENTER && used == 1);
    CHECK(decode("\n", &used) == PNL_KEY_ENTER && used == 1);
    CHECK(decode("q", &used) == 'q' && used == 1);
    CHECK(decode("\x03", &used) == 3 && used == 1);
    CHECK(decode("\xc3\xa7", &used) == 0xe7 && used == 2);
    CHECK(decode("\xe2\x82\xac", &used) == 0x20ac && used == 3);
    CHECK(decode("\xf0\x9f\x98\x80", &used) == 0x1f600 && used == 4);
    CHECK(decode("\xc0\x80", &used) == PNL_KEY_NONE && used == 1);
    CHECK(decode("\xe0\x80\x80", &used) == PNL_KEY_NONE && used == 3);
    CHECK(decode("\xed\xa0\x80", &used) == PNL_KEY_NONE && used == 3);
    CHECK(decode("\xe2\x28", &used) == PNL_KEY_NONE && used == 1);
    CHECK(pnl_key_decode(NULL, 0, &key) == 0 && key == PNL_KEY_NONE);
    CHECK(pnl_key_decode((const unsigned char *)"a", 1, NULL) == 0);
}

static void test_build(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_set_status(app, noop_status, NULL, 500) == PNL_OK);
    CHECK(pnl_app_add_action(app, "First", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_add_action(app, "Second", noop_action, NULL) == PNL_OK);
    pnl_app_destroy(app);
}

static void test_outside_callbacks(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_status_line(app, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_result(app, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    pnl_app_destroy(app);
}

static void test_no_terminal(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_add_action(app, "First", noop_action, NULL) == PNL_OK);
    CHECK(pnl_app_run(app) == PNL_ERR_TTY);
    pnl_app_destroy(app);
}

static void test_bad_args(void)
{
    pnl_app_t *app = NULL;

    CHECK(pnl_app_create("demo", failing_alloc, NULL, NULL, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create(NULL, NULL, NULL, NULL, &app) == PNL_ERR_ARG);
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_set_status(NULL, noop_status, NULL, 0) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(NULL, "x", noop_action, NULL) == PNL_ERR_ARG);
    CHECK(pnl_status_line(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_result(NULL, PNL_MARK_OK, "x") == PNL_ERR_ARG);
    CHECK(pnl_app_run(NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_create("demo", NULL, NULL, NULL, &app) == PNL_OK);
    CHECK(pnl_app_set_status(app, noop_status, NULL, -1) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(app, NULL, noop_action, NULL) == PNL_ERR_ARG);
    CHECK(pnl_app_add_action(app, "x", NULL, NULL) == PNL_ERR_ARG);
    pnl_app_destroy(app);
    CHECK(pnl_strerror(PNL_ERR_TTY) != NULL);
    CHECK(pnl_strerror(-1) != NULL);
    pnl_app_destroy(NULL);
}

static int run_with_failure(failing_alloc_t *fa)
{
    pnl_app_t *app = NULL;
    int status;
    int i;

    status = pnl_app_create("demo", failing_alloc, failing_free, fa, &app);
    for (i = 0; i < 20 && status == PNL_OK; i++)
        status = pnl_app_add_action(app, "an action label", noop_action, NULL);
    pnl_app_destroy(app);
    return status;
}

static void test_alloc_failure(void)
{
    int n;

    for (n = 0;; n++) {
        failing_alloc_t fa = {n, 0};
        int status = run_with_failure(&fa);

        CHECK(fa.live == 0);
        if (status == PNL_OK)
            break;
        CHECK(status == PNL_ERR_NOMEM);
    }
    CHECK(n > 1);
}

int main(void)
{
    test_keys();
    test_build();
    test_outside_callbacks();
    test_no_terminal();
    test_bad_args();
    test_alloc_failure();
    return 0;
}
