#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_ROLE_FIRST PNL_ROLE_ACCENT
#define PNL_CONTRAST_TEXT 4.5
#define PNL_CONTRAST_MUTED 3.0
#define PNL_DARK_BACKGROUND 0x1c1c1cL
#define PNL_LIGHT_BACKGROUND 0xf5f5f5L

struct pnl_role_ink {
    unsigned long rgb;
    int x256;
    int basic;
};

static const struct pnl_role_ink pnl_roles_dark[PNL_ROLES] = {
    {0x5fafffUL, 75, PNL_COLOR_CYAN},
    {0x5fd75fUL, 77, PNL_COLOR_GREEN},
    {0xd7af00UL, 178, PNL_COLOR_YELLOW},
    {0xff5f5fUL, 203, PNL_COLOR_RED},
    {0x8a8a8aUL, 245, PNL_COLOR_GRAY},
    {0x87afd7UL, 110, PNL_COLOR_BLUE},
    {0x3a3a3aUL, 237, PNL_COLOR_GRAY},
};

static const struct pnl_role_ink pnl_roles_light[PNL_ROLES] = {
    {0x005fd7UL, 26, PNL_COLOR_BLUE},
    {0x008700UL, 28, PNL_COLOR_GREEN},
    {0x875f00UL, 94, PNL_COLOR_YELLOW},
    {0xd70000UL, 160, PNL_COLOR_RED},
    {0x6c6c6cUL, 242, PNL_COLOR_GRAY},
    {0x005f87UL, 24, PNL_COLOR_BLUE},
    {0xdadadaUL, 253, PNL_COLOR_WHITE},
};

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

void pnl_palette_init(struct pnl_palette *palette, int depth, int light, long background_rgb)
{
    const struct pnl_role_ink *roles = light ? pnl_roles_light : pnl_roles_dark;
    unsigned long background = background_rgb >= 0 ? (unsigned long)background_rgb
                                                   : (unsigned long)(light ? PNL_LIGHT_BACKGROUND : PNL_DARK_BACKGROUND);
    int i;

    palette->depth = depth;
    palette->light = light;
    for (i = 0; i < PNL_ROLES; i++) {
        double minimum = i + PNL_ROLE_FIRST == PNL_ROLE_MUTED ? PNL_CONTRAST_MUTED : PNL_CONTRAST_TEXT;

        palette->rgb[i] = roles[i].rgb;
        palette->x256[i] = roles[i].x256;
        palette->basic[i] = roles[i].basic;
        if (i + PNL_ROLE_FIRST != PNL_ROLE_SURFACE)
            palette->rgb[i] = pnl_readable(roles[i].rgb, background, minimum);
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

int pnl_color_ink(char *buf, unsigned ink, int background, const struct pnl_palette *palette)
{
    int base = background ? 40 : 30;
    unsigned long rgb;
    int role;

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
