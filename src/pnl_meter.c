#include "pnl_internal.h"

#include <stdio.h>
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

static void pnl_bar_percent(pnl_canvas_t *canvas, int x, int y, int width, double fraction, int color)
{
    char text[8];
    int filled = (int)(fraction * width + 0.5);
    int len = snprintf(text, sizeof(text), "%d%%", (int)(fraction * 100.0 + 0.5));
    int start = (width - len) / 2;
    int i;

    for (i = 0; i < len && start >= 0; i++) {
        char glyph[2] = {text[i], '\0'};
        int inside = start + i < filled;

        pnl_canvas_text(canvas, x + start + i, y,
                        PNL_BOLD | (inside ? PNL_BG(color) | PNL_FG(PNL_ROLE_SURFACE)
                                           : PNL_BG(PNL_ROLE_SURFACE) | PNL_FG(color)),
                        glyph);
    }
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
    if (flags & PNL_METER_PERCENT)
        pnl_bar_percent(canvas, x, y, width, fraction, color);
}

static void pnl_column(pnl_canvas_t *canvas, int x, int bottom, int width, int height, double fraction, int flags)
{
    int eighths = (int)(pnl_clamp(fraction) * height * 8 + 0.5);
    int row;

    for (row = 0; row < height && eighths > row * 8; row++) {
        int part = eighths - row * 8 >= 8 ? 7 : eighths - row * 8 - 1;
        double level = flags & PNL_METER_LEVEL ? (double)(row + 1) / height : 0.0;
        int i;

        for (i = 0; i < width; i++)
            pnl_canvas_text(canvas, x + i, bottom - row, PNL_FG(pnl_meter_color(level, flags)), pnl_spark_blocks[part]);
    }
}

void pnl_canvas_chart(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                      size_t values_len, double max, int flags)
{
    size_t first;
    size_t i;

    if (canvas == NULL || values == NULL || width <= 0 || height <= 0 || !(max > 0.0))
        return;
    first = values_len > (size_t)width ? values_len - (size_t)width : 0;
    for (i = first; i < values_len; i++)
        pnl_column(canvas, x + (int)(i - first), y + height - 1, 1, height, values[i] / max, flags);
}

void pnl_canvas_columns(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                        const char *const *labels, size_t count, double max, int flags)
{
    int slot;
    int bars = labels != NULL ? height - 1 : height;
    size_t i;

    if (canvas == NULL || values == NULL || count == 0 || width < (int)count || bars <= 0 || !(max > 0.0))
        return;
    slot = width / (int)count;
    for (i = 0; i < count; i++) {
        int left = x + (int)i * slot;
        int bar = slot > 2 ? slot - 1 : slot;

        pnl_column(canvas, left, y + bars - 1, bar, bars, values[i] / max, flags);
        if (labels != NULL && labels[i] != NULL)
            pnl_canvas_clip(canvas, left, y + bars, bar, PNL_FG(PNL_ROLE_MUTED), labels[i]);
    }
}

static const unsigned char pnl_braille_bits[2][4] = {{0x01, 0x02, 0x04, 0x40}, {0x08, 0x10, 0x20, 0x80}};

static int pnl_plot_dot(const double *values, size_t at, double min, double max, int rows)
{
    double fraction = pnl_clamp((values[at] - min) / (max - min));

    return (int)((1.0 - fraction) * (rows * 4 - 1) + 0.5);
}

static void pnl_plot_cell(pnl_canvas_t *canvas, int x, int y, int height, const unsigned char *bits, int color)
{
    int row;

    for (row = 0; row < height; row++) {
        char glyph[4];

        if (bits[row] == 0)
            continue;
        glyph[pnl_utf8_put(0x2800u + bits[row], glyph)] = '\0';
        pnl_canvas_text(canvas, x, y + row, PNL_FG(color), glyph);
    }
}

void pnl_canvas_plot(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values,
                     size_t values_len, double min, double max, int flags)
{
    unsigned char bits[PNL_PLOT_ROWS];
    size_t first;
    size_t dot;
    int previous = -1;

    if (canvas == NULL || values == NULL || width <= 0 || height <= 0 || height > PNL_PLOT_ROWS || !(max > min))
        return;
    first = values_len > (size_t)width * 2 ? values_len - (size_t)width * 2 : 0;
    memset(bits, 0, sizeof(bits));
    for (dot = first; dot < values_len; dot++) {
        int column = (int)((dot - first) % 2);
        int at = pnl_plot_dot(values, dot, min, max, height);
        int from = previous < 0 ? at : previous;
        int step = from <= at ? 1 : -1;
        int row;

        for (row = from; row != at + step; row += step)
            bits[row / 4] |= pnl_braille_bits[column][row % 4];
        previous = at;
        if (column == 1 || dot + 1 == values_len) {
            double fraction = pnl_clamp((values[dot] - min) / (max - min));

            pnl_plot_cell(canvas, x + (int)(dot - first) / 2, y, height, bits, pnl_meter_color(fraction, flags));
            memset(bits, 0, sizeof(bits));
        }
    }
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
