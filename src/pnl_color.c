#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_ROLE_FIRST PNL_ROLE_ACCENT
#define PNL_CONTRAST_TEXT 4.5
#define PNL_CONTRAST_MUTED 3.0
#define PNL_CONTRAST_HIGH 7.0
#define PNL_DARK_BACKGROUND 0x1c1c1cL
#define PNL_LIGHT_BACKGROUND 0xf5f5f5L

static double pnl_channel(unsigned long rgb, int shift)
{
    double c = (double)((rgb >> shift) & 0xff) / 255.0;

    return c * c;
}

static double pnl_luminance(unsigned long rgb)
{
    return 0.2126 * pnl_channel(rgb, 16) + 0.7152 * pnl_channel(rgb, 8) + 0.0722 * pnl_channel(rgb, 0);
}

static double pnl_contrast(unsigned long a, unsigned long b)
{
    double la = pnl_luminance(a) + 0.05;
    double lb = pnl_luminance(b) + 0.05;

    return la > lb ? la / lb : lb / la;
}

static unsigned long pnl_mix(unsigned long rgb, unsigned long target, int percent)
{
    unsigned long out = 0;
    int shift;

    for (shift = 0; shift <= 16; shift += 8) {
        long from = (long)((rgb >> shift) & 0xff);
        long to = (long)((target >> shift) & 0xff);

        out |= (unsigned long)(from + (to - from) * percent / 100) << shift;
    }
    return out;
}

static unsigned long pnl_readable(unsigned long rgb, unsigned long background, double minimum)
{
    unsigned long target = pnl_luminance(background) > 0.25 ? 0x000000UL : 0xffffffUL;
    int steps;

    for (steps = 0; steps < 20 && pnl_contrast(rgb, background) < minimum; steps++)
        rgb = pnl_mix(rgb, target, 15);
    return rgb;
}

int pnl_is_light(long background_rgb)
{
    return pnl_luminance((unsigned long)background_rgb) > 0.25;
}

static int pnl_cube_step(unsigned long value)
{
    return value < 48 ? 0 : value < 115 ? 1 : (int)((value - 35) / 40);
}

static unsigned long pnl_cube_value(int step)
{
    return step == 0 ? 0 : (unsigned long)(55 + step * 40);
}

static unsigned long pnl_distance(unsigned long a, unsigned long b)
{
    unsigned long sum = 0;
    int shift;

    for (shift = 0; shift <= 16; shift += 8) {
        long d = (long)((a >> shift) & 0xff) - (long)((b >> shift) & 0xff);

        sum += (unsigned long)(d * d);
    }
    return sum;
}

int pnl_rgb_to_256(unsigned long rgb)
{
    int r = pnl_cube_step((rgb >> 16) & 0xff);
    int g = pnl_cube_step((rgb >> 8) & 0xff);
    int b = pnl_cube_step(rgb & 0xff);
    unsigned long cube = pnl_cube_value(r) << 16 | pnl_cube_value(g) << 8 | pnl_cube_value(b);
    unsigned long average = (((rgb >> 16) & 0xff) + ((rgb >> 8) & 0xff) + (rgb & 0xff)) / 3;
    int gray = average > 238 ? 23 : average < 8 ? 0 : (int)((average - 8) / 10);
    unsigned long level = (unsigned long)(8 + gray * 10);

    if (pnl_distance(rgb, level << 16 | level << 8 | level) < pnl_distance(rgb, cube))
        return 232 + gray;
    return 16 + 36 * r + 6 * g + b;
}

void pnl_palette_init(struct pnl_palette *palette, const struct pnl_theme *theme, int depth, int light,
                      long background_rgb, int high_contrast)
{
    const unsigned long *roles = light ? theme->light : theme->dark;
    unsigned long background = background_rgb >= 0 ? (unsigned long)background_rgb
                                                   : (unsigned long)(light ? PNL_LIGHT_BACKGROUND : PNL_DARK_BACKGROUND);
    int i;

    palette->depth = depth;
    palette->light = light;
    palette->background = background;
    for (i = 0; i < PNL_ROLES; i++) {
        double minimum = i + PNL_ROLE_FIRST == PNL_ROLE_MUTED ? PNL_CONTRAST_MUTED : PNL_CONTRAST_TEXT;

        if (high_contrast)
            minimum = minimum < PNL_CONTRAST_TEXT ? PNL_CONTRAST_TEXT : PNL_CONTRAST_HIGH;

        palette->rgb[i] = roles[i];
        if (i + PNL_ROLE_FIRST != PNL_ROLE_SURFACE)
            palette->rgb[i] = pnl_readable(roles[i], background, minimum);
        palette->x256[i] = pnl_rgb_to_256(palette->rgb[i]);
        palette->basic[i] = theme->basic[i];
    }
}

static int pnl_env_has(const char *name, const char *needle)
{
    const char *value = getenv(name);

    return value != NULL && (strstr)(value, needle) != NULL;
}

int pnl_color_detect(void)
{
    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");

    if (no_color != NULL && no_color[0] != '\0')
        return PNL_DEPTH_NONE;
    if (term != NULL && strcmp(term, "dumb") == 0)
        return PNL_DEPTH_NONE;
    if (pnl_env_has("COLORTERM", "truecolor") || pnl_env_has("COLORTERM", "24bit"))
        return PNL_DEPTH_TRUE;
#if defined(_WIN32)
    if (term == NULL)
        return PNL_DEPTH_TRUE;
#endif
    if (pnl_env_has("TERM", "256color") || pnl_env_has("TERM", "direct"))
        return PNL_DEPTH_256;
    return PNL_DEPTH_16;
}

int pnl_color_depth(void)
{
    static const int counts[] = {0, 16, 256, 16777216};

    return counts[pnl_color_detect()];
}

static int pnl_ink_basic(char *buf, unsigned ink, int base, int bright)
{
    if (ink == PNL_COLOR_DEFAULT || ink > PNL_COLOR_BRIGHT_WHITE)
        return sprintf(buf, ";%d", base + 9);
    if (ink <= 8)
        return sprintf(buf, ";%d", base + (int)ink - 1);
    return sprintf(buf, ";%d", bright + (int)ink - 9);
}

static unsigned long pnl_level_rgb(const struct pnl_palette *palette, int step)
{
    unsigned long ok = palette->rgb[PNL_ROLE_OK - PNL_ROLE_FIRST];
    unsigned long warn = palette->rgb[PNL_ROLE_WARN - PNL_ROLE_FIRST];
    unsigned long fail = palette->rgb[PNL_ROLE_FAIL - PNL_ROLE_FIRST];
    int half = PNL_LEVEL_STEPS / 2;

    if (step <= half)
        return pnl_mix(ok, warn, step * 100 / half);
    return pnl_mix(warn, fail, (step - half) * 100 / (PNL_LEVEL_STEPS - 1 - half));
}

static int pnl_level_ink(char *buf, int step, int base, const struct pnl_palette *palette)
{
    unsigned long rgb = pnl_level_rgb(palette, step);
    int basic = step * 3 < PNL_LEVEL_STEPS ? PNL_COLOR_GREEN : step * 3 < PNL_LEVEL_STEPS * 2 ? PNL_COLOR_YELLOW
                                                                                             : PNL_COLOR_RED;

    if (palette->depth == PNL_DEPTH_TRUE)
        return sprintf(buf, ";%d;2;%lu;%lu;%lu", base + 8, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    if (palette->depth == PNL_DEPTH_256)
        return sprintf(buf, ";%d;5;%d", base + 8, pnl_rgb_to_256(rgb));
    return pnl_ink_basic(buf, (unsigned)basic, base, base + 60);
}

static int pnl_fade_ink(char *buf, int fade, int base, const struct pnl_palette *palette)
{
    int role = fade / PNL_FADE_STEPS;
    int step = fade % PNL_FADE_STEPS;
    unsigned long rgb = pnl_mix(palette->rgb[role], palette->background, step * 100 / (PNL_FADE_STEPS - 1));

    if (palette->depth == PNL_DEPTH_TRUE)
        return sprintf(buf, ";%d;2;%lu;%lu;%lu", base + 8, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    return sprintf(buf, ";%d;5;%d", base + 8, pnl_rgb_to_256(rgb));
}

int pnl_fade_style(pnl_app_t *app, const pnl_canvas_t *canvas, int role, long long start_ms, int from_step)
{
    double t = pnl_app_tween(app, start_ms, PNL_FADE_MS);
    int depth = canvas->screen->palette.depth;
    int step;

    if (t >= 1.0)
        return 0;
    if (depth != PNL_DEPTH_TRUE && depth != PNL_DEPTH_256)
        return t < 0.5 ? PNL_BOLD : 0;
    step = from_step + (int)(t * (PNL_FADE_STEPS - 1 - from_step));
    return PNL_BG(PNL_FADE_FIRST + (role - PNL_ROLE_FIRST) * PNL_FADE_STEPS + step);
}

static const unsigned long pnl_basic_rgb[] = {0x000000UL, 0xcd0000UL, 0x00cd00UL, 0xcdcd00UL, 0x0000eeUL, 0xcd00cdUL,
                                              0x00cdcdUL, 0xe5e5e5UL, 0x7f7f7fUL, 0xff0000UL, 0x00ff00UL, 0xffff00UL,
                                              0x5c5cffUL, 0xff00ffUL, 0x00ffffUL, 0xffffffUL};

static unsigned pnl_nearest_basic(unsigned long rgb)
{
    unsigned best = 0;
    unsigned i;

    for (i = 1; i < sizeof(pnl_basic_rgb) / sizeof(pnl_basic_rgb[0]); i++) {
        if (pnl_distance(rgb, pnl_basic_rgb[i]) < pnl_distance(rgb, pnl_basic_rgb[best]))
            best = i;
    }
    return best + 1;
}

static int pnl_custom_ink(char *buf, unsigned long rgb, int base, int depth)
{
    if (depth == PNL_DEPTH_TRUE)
        return sprintf(buf, ";%d;2;%lu;%lu;%lu", base + 8, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    if (depth == PNL_DEPTH_256)
        return sprintf(buf, ";%d;5;%d", base + 8, pnl_rgb_to_256(rgb));
    return pnl_ink_basic(buf, pnl_nearest_basic(rgb), base, base + 60);
}

int pnl_color_ink(char *buf, unsigned ink, int background, const struct pnl_palette *palette)
{
    int base = background ? 40 : 30;
    unsigned long rgb;
    int role;

    if (ink < 256 && palette->custom_used[ink])
        return pnl_custom_ink(buf, palette->custom[ink], base, palette->depth);
    if (ink >= PNL_FADE_FIRST && ink < PNL_FADE_FIRST + PNL_ROLES * PNL_FADE_STEPS)
        return pnl_fade_ink(buf, (int)ink - PNL_FADE_FIRST, base, palette);
    if (ink >= PNL_LEVEL_FIRST && ink < PNL_LEVEL_FIRST + PNL_LEVEL_STEPS)
        return pnl_level_ink(buf, (int)ink - PNL_LEVEL_FIRST, base, palette);
    if (ink < PNL_ROLE_FIRST || ink >= PNL_ROLE_FIRST + PNL_ROLES)
        return pnl_ink_basic(buf, ink, base, base + 60);
    role = (int)ink - PNL_ROLE_FIRST;
    rgb = palette->rgb[role];
    if (palette->depth == PNL_DEPTH_TRUE)
        return sprintf(buf, ";%d;2;%lu;%lu;%lu", base + 8, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    if (palette->depth == PNL_DEPTH_256)
        return sprintf(buf, ";%d;5;%d", base + 8, palette->x256[role]);
    return pnl_ink_basic(buf, (unsigned)palette->basic[role], base, base + 60);
}

static const int pnl_custom_ranges[3][2] = {{17, 31}, {240, 255}, {39, 63}};

static int pnl_custom_run(const struct pnl_palette *palette, int length, int gradient)
{
    int r;

    for (r = 0; r < 3; r++) {
        const int *range = pnl_custom_ranges[gradient ? 2 - r : r];
        int start;

        for (start = range[0]; start + length - 1 <= range[1]; start++) {
            int i = 0;

            while (i < length && !palette->custom_used[start + i])
                i++;
            if (i == length)
                return start;
        }
    }
    return -1;
}

int pnl_app_color(pnl_app_t *app, unsigned long rgb, int *out_color)
{
    struct pnl_palette *palette;
    int slot;
    int i;

    if (out_color != NULL)
        *out_color = PNL_COLOR_DEFAULT;
    if (app == NULL || out_color == NULL || rgb > 0xffffffUL)
        return PNL_ERR_ARG;
    palette = &app->screen.palette;
    for (i = 0; i < 256; i++) {
        if (palette->custom_used[i] == 1 && palette->custom[i] == rgb) {
            *out_color = i;
            return PNL_OK;
        }
    }
    slot = pnl_custom_run(palette, 1, 0);
    if (slot < 0)
        return PNL_ERR_LIMIT;
    palette->custom[slot] = rgb;
    palette->custom_used[slot] = 1;
    *out_color = slot;
    return PNL_OK;
}

static unsigned long pnl_gradient_rgb(const unsigned long *stops, size_t count, double t)
{
    double place = t * (double)(count - 1);
    size_t low = (size_t)place;

    if (low >= count - 1)
        return stops[count - 1];
    return pnl_mix(stops[low], stops[low + 1], (int)((place - (double)low) * 100.0 + 0.5));
}

int pnl_app_gradient(pnl_app_t *app, const unsigned long *stops, size_t count, int steps, int *out_gradient)
{
    struct pnl_palette *palette;
    int first;
    int i;

    if (out_gradient != NULL)
        *out_gradient = 0;
    if (app == NULL || stops == NULL || out_gradient == NULL || count < 2 || steps < 2 ||
        steps > PNL_GRADIENT_STEPS_MAX)
        return PNL_ERR_ARG;
    for (i = 0; i < (int)count; i++) {
        if (stops[i] > 0xffffffUL)
            return PNL_ERR_ARG;
    }
    palette = &app->screen.palette;
    first = pnl_custom_run(palette, steps, 1);
    if (first < 0)
        return PNL_ERR_LIMIT;
    for (i = 0; i < steps; i++) {
        palette->custom[first + i] = pnl_gradient_rgb(stops, count, (double)i / (double)(steps - 1));
        palette->custom_used[first + i] = 2;
    }
    *out_gradient = first | steps << 8;
    return PNL_OK;
}

int pnl_gradient_color(int gradient, double fraction)
{
    int first = gradient & 0xff;
    int steps = gradient >> 8 & 0xff;

    if (steps < 2)
        return first;
    fraction = fraction > 0.0 ? (fraction < 1.0 ? fraction : 1.0) : 0.0;
    return first + (int)(fraction * (steps - 1) + 0.5);
}

int pnl_canvas_gradient_text(pnl_canvas_t *canvas, int x, int y, int style, int gradient, const char *text)
{
    size_t count = 0;
    size_t index = 0;
    const char *p;

    if (canvas == NULL || text == NULL)
        return x;
    for (p = text; *p != '\0'; count++) {
        uint32_t cp;

        p += pnl_utf8_next(p, strlen(p), &cp);
    }
    for (p = text; *p != '\0';) {
        char glyph[8];
        uint32_t cp;
        size_t len = pnl_utf8_next(p, strlen(p), &cp);
        double t = count > 1 ? (double)index / (double)(count - 1) : 0.0;

        memcpy(glyph, p, len);
        glyph[len] = '\0';
        x = pnl_canvas_text(canvas, x, y, (style & ~0xff00) | PNL_FG(pnl_gradient_color(gradient, t)), glyph);
        p += len;
        index++;
    }
    return x;
}

void pnl_canvas_gradient_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int gradient)
{
    int i;

    if (canvas == NULL || width <= 0)
        return;
    for (i = 0; i < width; i++) {
        double t = width > 1 ? (double)i / (double)(width - 1) : 0.0;

        pnl_canvas_fill(canvas, x + i, y, 1, height, PNL_BG(pnl_gradient_color(gradient, t)));
    }
}
