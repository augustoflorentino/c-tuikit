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

int pnl_color_ink(char *buf, unsigned ink, int background, const struct pnl_palette *palette)
{
    int base = background ? 40 : 30;
    unsigned long rgb;
    int role;

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
