#include "pnl_internal.h"

#include <string.h>

static const char *const pnl_spark_blocks[] = {"\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
                                               "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"};

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
    int filled;

    if (canvas == NULL || width <= 0)
        return;
    fraction = pnl_clamp(fraction);
    filled = (int)(fraction * width + 0.5);
    pnl_canvas_fill(canvas, x, y, filled, 1, PNL_BG(pnl_meter_color(fraction, flags)));
    pnl_canvas_fill(canvas, x + filled, y, width - filled, 1, PNL_BG(PNL_ROLE_SURFACE));
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

int pnl_status_meter(pnl_app_t *app, int mark, const char *text, double fraction)
{
    int status = pnl_status_line(app, mark, text);

    if (status == PNL_OK)
        app->lines[app->lines_len - 1].meter = pnl_clamp(fraction);
    return status;
}
