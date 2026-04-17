#include "pnl_internal.h"

#include <string.h>

static const char *const pnl_spark_blocks[] = {"\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
                                               "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"};

static const char *const pnl_eighths[] = {"", "\xe2\x96\x8f", "\xe2\x96\x8e", "\xe2\x96\x8d",
                                          "\xe2\x96\x8c", "\xe2\x96\x8b", "\xe2\x96\x8a", "\xe2\x96\x89"};

static const char *const pnl_spin_frames[] = {"\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
                                              "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
                                              "\xe2\xa0\x87", "\xe2\xa0\x8f"};

#define PNL_SPIN_FRAMES (int)(sizeof(pnl_spin_frames) / sizeof(pnl_spin_frames[0]))

void pnl_draw_spinner(pnl_canvas_t *canvas, int x, int y, int style)
{
    canvas->screen->spinning = 1;
    pnl_canvas_text(canvas, x, y, style, pnl_spin_frames[canvas->screen->spin % PNL_SPIN_FRAMES]);
}

static double pnl_clamp(double fraction)
{
    if (!(fraction > 0.0))
        return 0.0;
    return fraction > 1.0 ? 1.0 : fraction;
}

int pnl_level_color(double fraction)
{
    return PNL_LEVEL_FIRST + (int)(pnl_clamp(fraction) * (PNL_LEVEL_STEPS - 1) + 0.5);
}

static int pnl_meter_color(double fraction, int flags)
{
    return flags & PNL_METER_LEVEL ? pnl_level_color(fraction) : PNL_ROLE_ACCENT;
}

void pnl_canvas_bar(pnl_canvas_t *canvas, int x, int y, int width, double fraction, int flags)
{
    int color;
    int eighths;
    int filled;

    if (canvas == NULL || width <= 0)
        return;
    fraction = pnl_clamp(fraction);
    color = pnl_meter_color(fraction, flags);
    eighths = (int)(fraction * width * 8 + 0.5);
    filled = eighths / 8;
    pnl_canvas_fill(canvas, x, y, filled, 1, PNL_BG(color));
    pnl_canvas_fill(canvas, x + filled, y, width - filled, 1, PNL_BG(PNL_ROLE_SURFACE));
    if (eighths % 8 != 0)
        pnl_canvas_text(canvas, x + filled, y, PNL_FG(color) | PNL_BG(PNL_ROLE_SURFACE), pnl_eighths[eighths % 8]);
}

void pnl_canvas_sparkline(pnl_canvas_t *canvas, int x, int y, int width, const double *values, size_t values_len,
                          double max, int flags)
{
    size_t first;
    size_t i;

    if (canvas == NULL || values == NULL || width <= 0 || !(max > 0.0))
        return;
    first = values_len > (size_t)width ? values_len - (size_t)width : 0;
    for (i = first; i < values_len; i++) {
        double fraction = pnl_clamp(values[i] / max);
        int block = (int)(fraction * 7 + 0.5);

        pnl_canvas_text(canvas, x + (int)(i - first), y, PNL_FG(pnl_meter_color(fraction, flags)),
                        pnl_spark_blocks[block]);
    }
}

int pnl_status_meter(pnl_app_t *app, int mark, const char *label, double fraction)
{
    int status = pnl_status_line(app, mark, label, "");

    if (status == PNL_OK)
        app->lines[app->lines_len - 1].meter = pnl_clamp(fraction);
    return status;
}
