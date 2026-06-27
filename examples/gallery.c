#include <c-tuikit/c-tuikit.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define TILES 6
#define TILE_COLUMNS_COUNT 3
#define HISTORY 240
#define LANES 3
#define TICK_MS 50
#define AUTO_TICKS 160

enum { TILE_CHART, TILE_PLOT, TILE_SPARKS, TILE_BARS, TILE_EQUALIZER, TILE_SPINNERS, TILE_COUNTER, TILE_KINDS };
enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW, WAVE_BOUNCE, WAVE_PULSE, WAVE_NOISE, WAVE_KINDS };

static const char *const tile_names[] = {"chart", "line", "sparklines", "bars", "equalizer", "spinners", "counter"};
static const char *const scheme_names[] = {"", ", heat", ", sunset", ", ocean", ", aurora", ", ember"};
static const unsigned long palettes[4][3] = {{0xffb347UL, 0xff5e62UL, 0x9b2fffUL},
                                             {0x00c6ffUL, 0x0072ffUL, 0x6a00f4UL},
                                             {0x43e97bUL, 0x38f9d7UL, 0xa17fe0UL},
                                             {0xf9d423UL, 0xff4e50UL, 0x8e0e00UL}};
static const char *const wave_names[] = {"sine", "square", "saw", "bounce", "heartbeat", "noise"};

struct gallery;

struct tile {
    struct gallery *gallery;
    int kind;
    int wave;
    int flags;
    int scheme;
    int lanes;
    double speed;
    double phase;
    double noise[LANES];
    double history[LANES][HISTORY];
    size_t len;
    char title[64];
};

struct gallery {
    pnl_app_t *app;
    struct tile tiles[TILES];
    pnl_widget_t *bodies[TILES];
    pnl_widget_t *titles[TILES];
    pnl_widget_t *cells[TILES];
    pnl_widget_t *rows[2];
    pnl_widget_t *grid;
    pnl_widget_t *screen;
    unsigned int seed;
    unsigned int variation;
    double clock;
    int ticks;
    int paused;
    int automatic;
    int timer;
    int gradients[4];
};

static unsigned int roll(struct gallery *gallery, unsigned int range)
{
    gallery->seed = gallery->seed * 1103515245u + 12345u;
    return (gallery->seed >> 16) % range;
}

static double turn(double x)
{
    return x - (double)(long)x;
}

static double sine(double x)
{
    double y = turn(x);
    double p;

    y = y < 0.5 ? y : y - 1.0;
    p = 8.0 * y - 16.0 * y * (y < 0.0 ? -y : y);
    return 0.225 * (p * (p < 0.0 ? -p : p) - p) + p;
}

static double clamp(double value)
{
    return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
}

static double wave_at(const struct tile *tile, int lane, double x)
{
    double f = turn(x);

    switch (tile->wave) {
    case WAVE_SINE:
        return 0.5 + 0.45 * sine(x);
    case WAVE_SQUARE:
        return clamp(0.5 + 1.6 * sine(x) * 0.5);
    case WAVE_SAW:
        return 0.05 + 0.9 * f;
    case WAVE_BOUNCE:
        return 0.05 + 0.9 * (sine(x * 0.5) < 0.0 ? -sine(x * 0.5) : sine(x * 0.5));
    case WAVE_PULSE:
        return f < 0.08 ? 0.1 + f / 0.08 * 0.85 : f < 0.16 ? 0.95 - (f - 0.08) / 0.08 * 0.85 : 0.1 + 0.03 * sine(x * 7);
    default:
        return tile->noise[lane];
    }
}

static double tile_value(const struct tile *tile, int lane, double clock)
{
    return wave_at(tile, lane, clock * tile->speed + tile->phase + lane * 0.21);
}

static void tile_title(struct tile *tile)
{
    if (tile->kind == TILE_SPINNERS) {
        snprintf(tile->title, sizeof(tile->title), "spinners and busy bars");
        return;
    }
    snprintf(tile->title, sizeof(tile->title), "%s of %s at %.1fx%s", tile_names[tile->kind],
             wave_names[tile->wave], tile->speed, scheme_names[tile->scheme]);
}

static void tile_roll(struct gallery *gallery, struct tile *tile)
{
    memset(tile, 0, sizeof(*tile));
    tile->gallery = gallery;
    tile->kind = (int)roll(gallery, TILE_KINDS);
    tile->wave = (int)roll(gallery, WAVE_KINDS);
    tile->scheme = (int)roll(gallery, 6);
    tile->flags = tile->scheme == 0   ? PNL_METER_ACCENT
                  : tile->scheme == 1 ? PNL_METER_LEVEL
                                      : PNL_METER_GRADIENT(gallery->gradients[tile->scheme - 2]);
    tile->lanes = tile->kind == TILE_SPARKS ? LANES : 1;
    tile->speed = 0.2 + (double)roll(gallery, 15) / 10.0;
    tile->phase = (double)roll(gallery, 100) / 100.0;
    tile->noise[0] = tile->noise[1] = tile->noise[2] = 0.5;
    tile_title(tile);
}

static void tile_step(struct gallery *gallery, struct tile *tile)
{
    int lane;

    for (lane = 0; lane < tile->lanes; lane++) {
        double drift = ((double)roll(gallery, 1001) / 1000.0 - 0.5) * 0.18;

        tile->noise[lane] = clamp(tile->noise[lane] + drift);
        if (tile->len == HISTORY)
            memmove(tile->history[lane], tile->history[lane] + 1, (HISTORY - 1) * sizeof(double));
        tile->history[lane][tile->len < HISTORY ? tile->len : HISTORY - 1] =
            tile_value(tile, lane, gallery->clock);
    }
    if (tile->len < HISTORY)
        tile->len++;
}

static void draw_counter(const struct tile *tile, pnl_canvas_t *canvas, double value, int height)
{
    int gradient = tile->scheme > 1 ? tile->gallery->gradients[tile->scheme - 2] : 0;
    int role = tile->scheme == 0 ? PNL_ROLE_ACCENT : value > 0.7 ? PNL_ROLE_FAIL : value > 0.4 ? PNL_ROLE_WARN : PNL_ROLE_OK;
    int size = height >= 8 ? PNL_BIG_LARGE : PNL_BIG_SMALL;
    int y = height > pnl_big_height(size) + 2 ? (height - pnl_big_height(size) - 2) / 2 : 0;
    char number[8];

    snprintf(number, sizeof(number), "%d%%", (int)(value * 100.0 + 0.5));
    pnl_canvas_big(canvas, 1, y, size, PNL_FG(role), gradient, number);
    pnl_canvas_bar(canvas, 1, y + pnl_big_height(size) + 1, pnl_big_width(size, "100%"), value, tile->flags);
}

static void draw_equalizer(const struct tile *tile, pnl_canvas_t *canvas, double clock, int width, int height)
{
    double values[16];
    int count = width / 3 < 16 ? width / 3 : 16;
    int i;

    for (i = 0; i < count; i++)
        values[i] = wave_at(tile, 0, clock * tile->speed + tile->phase + i * 0.13);
    pnl_canvas_columns(canvas, 0, 0, width, height, values, NULL, (size_t)count, 1.0, tile->flags | PNL_METER_LEVEL);
}

static void draw_bars(const struct tile *tile, pnl_canvas_t *canvas, double clock, int width, int height)
{
    int y;

    for (y = 0; y < height; y += 2)
        pnl_canvas_bar(canvas, 0, y, width, wave_at(tile, 0, clock * tile->speed + tile->phase + y * 0.07),
                       tile->flags | PNL_METER_PERCENT);
}

static void draw_spinners(pnl_canvas_t *canvas, int width, int height)
{
    int y;
    int kind;

    for (kind = 0; kind <= PNL_SPINNER_BOUNCE; kind++)
        pnl_canvas_spinner(canvas, kind * 3, 0, PNL_FG(PNL_ROLE_ACCENT), kind);
    for (y = 2; y < height; y += 2)
        pnl_canvas_busy(canvas, 0, y, width - (y / 2 - 1) * 4, y % 4 == 0 ? PNL_METER_LEVEL : PNL_METER_ACCENT);
}

static void draw_sparks(const struct tile *tile, pnl_canvas_t *canvas, int width, int height)
{
    int lane;

    for (lane = 0; lane < LANES && lane * 2 < height; lane++)
        pnl_canvas_sparkline(canvas, 0, lane * 2, width, tile->history[lane], tile->len, 1.0, tile->flags);
}

static void tile_body(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    const struct tile *tile = ctx;
    const struct gallery *gallery = tile->gallery;
    int width = pnl_canvas_width(canvas);
    int height = pnl_canvas_height(canvas);
    double now = tile->len > 0 ? tile->history[0][tile->len - 1] : 0.0;

    (void)app;

    if (tile->kind == TILE_CHART)
        pnl_canvas_chart(canvas, 0, 0, width, height, tile->history[0], tile->len, 1.0, tile->flags);
    else if (tile->kind == TILE_PLOT)
        pnl_canvas_plot(canvas, 0, 0, width, height, tile->history[0], tile->len, 0.0, 1.0, tile->flags);
    else if (tile->kind == TILE_SPARKS)
        draw_sparks(tile, canvas, width, height);
    else if (tile->kind == TILE_BARS)
        draw_bars(tile, canvas, gallery->clock, width, height);
    else if (tile->kind == TILE_EQUALIZER)
        draw_equalizer(tile, canvas, gallery->clock, width, height);
    else if (tile->kind == TILE_SPINNERS)
        draw_spinners(canvas, width, height);
    else
        draw_counter(tile, canvas, now, height);
}

static void shuffle(struct gallery *gallery)
{
    int i;

    gallery->variation++;
    for (i = 0; i < TILES; i++) {
        tile_roll(gallery, &gallery->tiles[i]);
        pnl_widget_set_text(gallery->titles[i], gallery->tiles[i].title);
    }
    gallery->ticks = 0;
}

static void tick(void *ctx, pnl_app_t *app)
{
    struct gallery *gallery = ctx;
    int i;

    (void)app;
    if (gallery->paused)
        return;
    gallery->clock += TICK_MS / 1000.0;
    for (i = 0; i < TILES; i++)
        tile_step(gallery, &gallery->tiles[i]);
    if (gallery->automatic && ++gallery->ticks >= AUTO_TICKS)
        shuffle(gallery);
}

static void gallery_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct gallery *gallery = ctx;
    char line[128];

    (void)app;
    snprintf(line, sizeof(line), "variation %u%s%s", gallery->variation, gallery->paused ? "  paused" : "",
             gallery->automatic ? "  changes by itself" : "");
    pnl_canvas_gradient_text(canvas, 0, 0, PNL_BOLD, gallery->gradients[gallery->variation % 4], line);
    if (gallery->automatic && !gallery->paused)
        pnl_canvas_bar(canvas, pnl_canvas_width(canvas) - 21, 0, 20, (double)gallery->ticks / AUTO_TICKS,
                       PNL_METER_ACCENT);
    pnl_widget_draw(gallery->grid, canvas, 0, 2, pnl_canvas_width(canvas), pnl_canvas_height(canvas) - 2);
}

static void show_toast(struct gallery *gallery)
{
    static const char *const texts[] = {"a toast slides in", "and slides away", "three can stack up"};
    static const int marks[] = {PNL_MARK_OK, PNL_MARK_WAIT, PNL_MARK_FAIL};
    unsigned int pick = roll(gallery, 3);

    pnl_toast(gallery->app, marks[pick], texts[pick]);
}

static int gallery_key(void *ctx, pnl_app_t *app, int key)
{
    struct gallery *gallery = ctx;
    int yes = 0;

    if (key == 'n' || key == PNL_KEY_ENTER)
        shuffle(gallery);
    else if (key == ' ')
        gallery->paused = !gallery->paused;
    else if (key == 'a')
        gallery->automatic = !gallery->automatic;
    else if (key == 't')
        show_toast(gallery);
    else if (key == 'x')
        pnl_view_shake(app);
    else if (key == 'd')
        pnl_confirm(app, "a dialog grows in; press x here to shake it", &yes);
    else
        return 0;
    return 1;
}

static const char *gallery_hints(void *ctx)
{
    (void)ctx;
    return "n: next  space: pause  a: auto  t: toast  d: dialog  x: shake";
}

static int build_cell(struct gallery *gallery, int i)
{
    static const pnl_widget_class_t body_class = {tile_body, NULL, NULL, NULL, NULL};
    int status;

    tile_roll(gallery, &gallery->tiles[i]);
    status = pnl_widget_create(gallery->app, &body_class, &gallery->tiles[i], &gallery->bodies[i]);
    if (status == PNL_OK)
        status = pnl_widget_text(gallery->app, gallery->tiles[i].title, PNL_BOLD | PNL_FG(PNL_ROLE_MUTED),
                                 &gallery->titles[i]);
    if (status == PNL_OK)
        status = pnl_widget_stack(gallery->app, PNL_STACK_ROWS, &gallery->cells[i]);
    if (status == PNL_OK)
        status = pnl_widget_add(gallery->cells[i], gallery->titles[i], 2);
    if (status == PNL_OK)
        status = pnl_widget_add(gallery->cells[i], gallery->bodies[i], 0);
    if (status == PNL_OK)
        status = pnl_widget_add(gallery->rows[i / TILE_COLUMNS_COUNT], gallery->cells[i], 0);
    return status;
}

static int build(struct gallery *gallery)
{
    static const pnl_widget_class_t gallery_class = {gallery_draw, gallery_key, NULL, gallery_hints, NULL};
    int status = pnl_widget_stack(gallery->app, PNL_STACK_ROWS, &gallery->grid);
    int i;

    for (i = 0; i < 2 && status == PNL_OK; i++) {
        status = pnl_widget_stack(gallery->app, PNL_STACK_COLUMNS, &gallery->rows[i]);
        if (status == PNL_OK)
            status = pnl_widget_add(gallery->grid, gallery->rows[i], 0);
    }
    for (i = 0; i < TILES && status == PNL_OK; i++)
        status = build_cell(gallery, i);
    if (status == PNL_OK)
        status = pnl_widget_create(gallery->app, &gallery_class, gallery, &gallery->screen);
    return status;
}

static void release(struct gallery *gallery)
{
    int i;

    pnl_widget_destroy(gallery->screen);
    for (i = 0; i < TILES; i++) {
        pnl_widget_destroy(gallery->cells[i]);
        pnl_widget_destroy(gallery->titles[i]);
        pnl_widget_destroy(gallery->bodies[i]);
    }
    pnl_widget_destroy(gallery->rows[0]);
    pnl_widget_destroy(gallery->rows[1]);
    pnl_widget_destroy(gallery->grid);
}

static int register_gradients(struct gallery *gallery)
{
    int status = PNL_OK;
    int i;

    for (i = 0; i < 4 && status == PNL_OK; i++)
        status = pnl_app_gradient(gallery->app, palettes[i], 3, 12, &gallery->gradients[i]);
    return status;
}

static void open_gallery(void *ctx, pnl_app_t *app)
{
    struct gallery *gallery = ctx;

    pnl_view_push_widget(app, "gallery", gallery->screen);
}

int main(int argc, char **argv)
{
    static struct gallery gallery;
    int exit_code = 1;
    int status;

    gallery.seed = (unsigned int)time(NULL);
    gallery.automatic = 1;
    status = pnl_app_create("gallery", NULL, &gallery.app);
    if (status == PNL_OK)
        status = pnl_app_enable_motion(gallery.app, PNL_MOTION_SLIDE | PNL_MOTION_TRANSITION | PNL_MOTION_PULSE |
                                                        PNL_MOTION_ENTER | PNL_MOTION_FEEDBACK | PNL_MOTION_COUNT);
    if (status == PNL_OK)
        status = register_gradients(&gallery);
    if (status == PNL_OK)
        status = pnl_app_set_banner(gallery.app, NULL, gallery.gradients[0]);
    if (status == PNL_OK)
        status = build(&gallery);
    if (status == PNL_OK)
        status = pnl_app_add_timer(gallery.app, TICK_MS, tick, &gallery, &gallery.timer);
    if (status == PNL_OK)
        status = pnl_app_add_action(gallery.app, "Gallery", open_gallery, &gallery);
    if (status == PNL_OK)
        status = pnl_app_run(gallery.app, argc, argv, &exit_code);
    if (status != PNL_OK)
        fprintf(stderr, "error: %s\n", pnl_strerror(status));
    release(&gallery);
    pnl_app_destroy(gallery.app);
    return exit_code;
}
