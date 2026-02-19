#include "pnl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNL_ROLE_FIRST PNL_ROLE_ACCENT
#define PNL_ROLE_COUNT (PNL_ROLE_SURFACE - PNL_ROLE_ACCENT + 1)

struct pnl_role_ink {
    unsigned long rgb;
    int x256;
    int basic;
};

static const struct pnl_role_ink pnl_roles[PNL_ROLE_COUNT] = {
    {0x5fafffUL, 75, PNL_COLOR_CYAN},
    {0x5fd75fUL, 77, PNL_COLOR_GREEN},
    {0xd7af00UL, 178, PNL_COLOR_YELLOW},
    {0xff5f5fUL, 203, PNL_COLOR_RED},
    {0x8a8a8aUL, 245, PNL_COLOR_GRAY},
    {0x87afd7UL, 110, PNL_COLOR_BLUE},
    {0x3a3a3aUL, 237, PNL_COLOR_GRAY},
};

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

int pnl_color_ink(char *buf, unsigned ink, int background, int depth)
{
    const struct pnl_role_ink *role;
    int base = background ? 40 : 30;

    if (ink < PNL_ROLE_FIRST || ink >= PNL_ROLE_FIRST + PNL_ROLE_COUNT)
        return pnl_ink_basic(buf, ink, base, base + 60);
    role = &pnl_roles[ink - PNL_ROLE_FIRST];
    if (depth == PNL_DEPTH_TRUE)
        return sprintf(buf, ";%d;2;%lu;%lu;%lu", base + 8, (role->rgb >> 16) & 0xff, (role->rgb >> 8) & 0xff,
                       role->rgb & 0xff);
    if (depth == PNL_DEPTH_256)
        return sprintf(buf, ";%d;5;%d", base + 8, role->x256);
    return pnl_ink_basic(buf, (unsigned)role->basic, base, base + 60);
}
