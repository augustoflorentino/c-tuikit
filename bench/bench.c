#define _POSIX_C_SOURCE 199309L

#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 200
#define HEIGHT 60
#define ROWS 1000000

struct counter {
    size_t bytes;
    size_t peak;
    size_t calls;
};

struct block {
    size_t size;
    long long align;
};

static void *count_alloc(void *ctx, size_t size)
{
    struct counter *counter = ctx;
    struct block *block = malloc(sizeof(*block) + size);

    if (block == NULL)
        return NULL;
    block->size = size;
    counter->bytes += size;
    counter->calls++;
    if (counter->bytes > counter->peak)
        counter->peak = counter->bytes;
    return block + 1;
}

static void count_free(void *ctx, void *ptr)
{
    struct counter *counter = ctx;
    struct block *block = ptr;

    if (ptr == NULL)
        return;
    block--;
    counter->bytes -= block->size;
    free(block);
}

static double now_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void report(const char *name, double total_us, double count, const char *unit, double extra,
                   const char *extra_unit)
{
    double each = total_us / count;

    if (each >= 1000.0)
        printf("%-30s %10.2f ms%s", name, each / 1000.0, unit);
    else if (each >= 1.0)
        printf("%-30s %10.2f us%s", name, each, unit);
    else
        printf("%-30s %10.1f ns%s", name, each * 1000.0, unit);
    if (extra_unit != NULL)
        printf("   %.0f %s", extra, extra_unit);
    printf("\n");
}

static void fill_screen(struct pnl_screen *screen)
{
    char line[WIDTH + 1];
    int y;
    int x;

    pnl_screen_clear(screen);
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++)
            line[x] = (char)('a' + (x * 7 + y * 3) % 26);
        line[WIDTH] = '\0';
        pnl_screen_text(screen, 0, y, -1, (uint32_t)((y % 8) << 8), line);
    }
}

static void bench_screen(int linear)
{
    struct pnl_mem mem = {count_alloc, count_free, NULL};
    struct counter counter = {0, 0, 0};
    struct pnl_screen *screen = calloc(1, sizeof(*screen));
    struct pnl_theme theme;
    size_t bytes = 0;
    double start;
    int frames = linear ? 200 : 1000;
    int i;

    mem.ctx = &counter;
    pnl_theme_choose(&theme, "default");
    pnl_palette_init(&screen->palette, &theme, PNL_DEPTH_TRUE, 0, -1, 0);
    screen->linear = linear;
    pnl_screen_resize(screen, &mem, WIDTH, HEIGHT);
    start = now_us();
    for (i = 0; i < frames; i++)
        fill_screen(screen);
    report("draw 200x60 text", now_us() - start, frames, "/frame", 0, NULL);
    start = now_us();
    for (i = 0; i < frames; i++) {
        fill_screen(screen);
        screen->front_valid = 0;
        pnl_screen_render(screen, &mem);
        bytes += screen->out_len;
    }
    report(linear ? "draw + linear full frame" : "draw + render full frame", now_us() - start, frames, "/frame",
           (double)bytes / frames, "bytes/frame");
    bytes = 0;
    fill_screen(screen);
    pnl_screen_render(screen, &mem);
    start = now_us();
    for (i = 0; i < frames; i++) {
        char text[32];

        snprintf(text, sizeof(text), "frame %d", i);
        pnl_screen_text(screen, 0, 5, -1, 0, text);
        pnl_screen_render(screen, &mem);
        bytes += screen->out_len;
    }
    report(linear ? "linear one changed row" : "render one changed row", now_us() - start, frames, "/frame",
           (double)bytes / frames, "bytes/frame");
    pnl_screen_free(screen, &mem);
    free(screen);
}

static void bench_log(pnl_app_t *app)
{
    pnl_log_t *logbook = NULL;
    double start;
    int i;

    pnl_log_create(app, 20000, &logbook);
    start = now_us();
    for (i = 0; i < ROWS; i++)
        pnl_log_write(logbook, PNL_LEVEL_INFO, "request served in 12 ms from cache");
    report("log write", now_us() - start, ROWS, "/line", 0, NULL);
    pnl_log_destroy(logbook);
}

static void bench_value(pnl_app_t *app)
{
    pnl_value_t *value = NULL;
    double start;
    int i;

    pnl_value_create(app, "load", &value);
    start = now_us();
    for (i = 0; i < ROWS; i++)
        pnl_value_set(value, PNL_MARK_OK, i % 2 ? "running" : "starting");
    report("value set", now_us() - start, ROWS, "/set", 0, NULL);
    pnl_value_destroy(value);
}

static const char *row_cell(void *ctx, size_t row, size_t column, char *buf, size_t buf_len)
{
    (void)ctx;
    snprintf(buf, buf_len, "%s-%07zu", column == 0 ? "file" : "size", (row * 7919u) % ROWS);
    return buf;
}

static void open_table(void *ctx, pnl_app_t *app)
{
    static const char *const headers[] = {"name", "size"};

    (void)ctx;
    pnl_table_source(app, "rows", headers, NULL, 2, ROWS, row_cell, NULL, NULL);
}

static double settle(pnl_test_t *test, const char *busy)
{
    double start = now_us();

    while (strstr(pnl_test_screen(test), busy) != NULL)
        pnl_test_wait(test, 0);
    return now_us() - start;
}

static void bench_table(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;
    double start;

    pnl_app_create("bench", NULL, &app);
    pnl_app_add_action(app, "Rows", open_table, NULL);
    pnl_test_create(app, 120, 40, &test);
    start = now_us();
    pnl_test_keys(test, "enter");
    report("table open 1M rows", now_us() - start, 1, "", 0, NULL);
    start = now_us();
    pnl_test_keys(test, "s");
    report("table sort 1M rows", now_us() - start + settle(test, "sorting"), 1, "", 0, NULL);
    start = now_us();
    pnl_test_keys(test, "/ \"12\" enter");
    report("table filter 1M rows", now_us() - start + settle(test, "filtering"), 1, "", 0, NULL);
    pnl_test_destroy(test);
    pnl_app_destroy(app);
}

static void noop_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

static void bench_memory(void)
{
    struct counter counter = {0, 0, 0};
    pnl_allocator_t allocator = {count_alloc, count_free, NULL};
    pnl_app_t *app = NULL;
    pnl_value_t *values[100];
    pnl_log_t *logbook = NULL;
    size_t before;
    int i;

    allocator.ctx = &counter;
    pnl_app_create("bench", &allocator, &app);
    printf("%-30s %10zu bytes\n", "app", counter.bytes);
    before = counter.bytes;
    for (i = 0; i < 100; i++)
        pnl_app_add_action(app, "an action", noop_action, NULL);
    printf("%-30s %10zu bytes\n", "action", (counter.bytes - before) / 100);
    before = counter.bytes;
    for (i = 0; i < 100; i++)
        pnl_value_create(app, "a value", &values[i]);
    printf("%-30s %10zu bytes\n", "value", (counter.bytes - before) / 100);
    before = counter.bytes;
    pnl_log_create(app, 1000, &logbook);
    printf("%-30s %10zu bytes\n", "log of 1000 lines, empty", counter.bytes - before);
    for (i = 0; i < 1000; i++)
        pnl_log_write(logbook, PNL_LEVEL_INFO, "request served in 12 ms from cache");
    printf("%-30s %10zu bytes\n", "log of 1000 lines, full", counter.bytes - before);
    pnl_log_destroy(logbook);
    for (i = 0; i < 100; i++)
        pnl_value_destroy(values[i]);
    pnl_app_destroy(app);
    printf("%-30s %10zu bytes\n", "left after destroy", counter.bytes);
}

int main(void)
{
    pnl_app_t *app = NULL;
    pnl_test_t *test = NULL;

    printf("c-tuikit bench\n\n");
    pnl_app_create("bench", NULL, &app);
    pnl_test_create(app, 40, 10, &test);
    bench_screen(0);
    bench_screen(1);
    pnl_test_destroy(test);
    bench_log(app);
    bench_value(app);
    pnl_app_destroy(app);
    bench_table();
    printf("\n");
    bench_memory();
    return 0;
}
