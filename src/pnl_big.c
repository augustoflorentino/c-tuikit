#include "pnl_internal.h"

#include <string.h>

#define PNL_GLYPH_ROWS 5
#define PNL_BANNER_ROOM 8

struct pnl_glyph {
    char key;
    const char *rows;
};

static const struct pnl_glyph pnl_glyphs[] = {
    {'0', "#### ## ## ####"}, {'1', " # ##  #  # ###"}, {'2', "###  #####  ###"}, {'3', "###  ####  ####"},
    {'4', "# ## ####  #  #"}, {'5', "####  ###  ####"}, {'6', "####  #### ####"}, {'7', "###  #  #  #  #"},
    {'8', "#### ##### ####"}, {'9', "#### ####  ####"}, {'A', " # # ##### ## #"}, {'B', "## # ### # ### "},
    {'C', " ###  #  #   ##"}, {'D', "## # ## ## ### "}, {'E', "####  ## #  ###"}, {'F', "####  ## #  #  "},
    {'G', " ###  # ## # ##"}, {'H', "# ## ##### ## #"}, {'I', "### #  #  # ###"}, {'J', "  #  #  ## # # "},
    {'K', "# ## ### # ## #"}, {'L', "#  #  #  #  ###"}, {'M', "# ######## ## #"}, {'N', "## # ## ## ## #"},
    {'O', " # # ## ## # # "}, {'P', "## # ### #  #  "}, {'Q', " # # ## ###  ##"}, {'R', "## # ### # ## #"},
    {'S', " ###   #   ### "}, {'T', "### #  #  #  # "}, {'U', "# ## ## ## ####"}, {'V', "# ## ## ## # # "},
    {'W', "# ## ######## #"}, {'X', "# ## # # # ## #"}, {'Y', "# ## # #  #  # "}, {'Z', "###  # # #  ###"},
    {' ', "               "}, {'.', "             # "}, {',', "          #  # "}, {':', "    #     #    "},
    {';', "    #     #  # "}, {'-', "      ###      "}, {'+', "    # ### #    "}, {'=', "   ###   ###   "},
    {'_', "            ###"}, {'%', "# #  # # #  # #"}, {'/', "  #  # # #  #  "}, {'!', " #  #  #     # "},
    {'?', "###  # ##    # "}, {'(', "  # #  #  #   #"}, {')', "#   #  #  # #  "}, {'<', "  # # #   #   #"},
    {'>', "#   #   # # #  "}, {'*', "   # # # # #   "}, {'#', "# ##### ##### #"}, {'\'', " #  #          "},
    {'"', "# ## #         "}, {'$', " ####  #  #### "}, {'@', "#### #####  ###"}, {'|', " #  #  #  #  # "},
};

static const char *pnl_glyph_rows(int c)
{
    size_t i;

    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    for (i = 0; i < sizeof(pnl_glyphs) / sizeof(pnl_glyphs[0]); i++) {
        if (pnl_glyphs[i].key == c)
            return pnl_glyphs[i].rows;
    }
    return NULL;
}

static int pnl_glyph_narrow(int c)
{
    return c != '\0' && (strchr)(".,:;!'|", c) != NULL;
}

static int pnl_pixel(const char *rows, int narrow, int column, int row)
{
    if (row >= PNL_GLYPH_ROWS)
        return 0;
    return rows[row * 3 + (narrow ? 1 : column)] == '#';
}

static int pnl_glyph_columns(int c)
{
    if (c == ' ')
        return 2;
    return pnl_glyph_narrow(c) ? 1 : 3;
}

static int pnl_big_scale(int size)
{
    return size == PNL_BIG_LARGE ? 2 : 1;
}

int pnl_big_height(int size)
{
    return size == PNL_BIG_LARGE ? PNL_GLYPH_ROWS : (PNL_GLYPH_ROWS + 1) / 2;
}

int pnl_big_width(int size, const char *text)
{
    int columns = 0;
    const char *p;

    if (text == NULL || (size != PNL_BIG_SMALL && size != PNL_BIG_LARGE))
        return 0;
    for (p = text; *p != '\0'; p++) {
        int c = pnl_glyph_rows((unsigned char)*p) != NULL ? (unsigned char)*p : '?';

        columns += pnl_glyph_columns(c) + (p[1] != '\0');
    }
    return columns * pnl_big_scale(size);
}

static int pnl_big_ink(int style, int gradient, int column, int width)
{
    if (gradient == 0)
        return style;
    return (style & ~0xff00) | PNL_FG(pnl_gradient_color(gradient, width > 1 ? (double)column / (width - 1) : 0.0));
}

static void pnl_big_cell(pnl_canvas_t *canvas, int x, int y, int style, int top, int bottom)
{
    if (top && bottom)
        pnl_canvas_text(canvas, x, y, style, "\xe2\x96\x88");
    else if (top)
        pnl_canvas_text(canvas, x, y, style, "\xe2\x96\x80");
    else if (bottom)
        pnl_canvas_text(canvas, x, y, style, "\xe2\x96\x84");
}

static void pnl_big_glyph(pnl_canvas_t *canvas, int x, int y, int size, int style, int gradient, int start,
                          int width, int c)
{
    const char *rows = pnl_glyph_rows(c);
    int narrow = pnl_glyph_narrow(c);
    int columns = pnl_glyph_columns(c);
    int scale = pnl_big_scale(size);
    int column;
    int row;

    for (column = 0; column < columns && c != ' '; column++) {
        int ink = pnl_big_ink(style, gradient, start + column * scale, width);

        for (row = 0; row < pnl_big_height(size); row++) {
            if (size == PNL_BIG_LARGE && pnl_pixel(rows, narrow, column, row))
                pnl_canvas_text(canvas, x + column * 2, y + row, ink, "\xe2\x96\x88\xe2\x96\x88");
            else if (size == PNL_BIG_SMALL)
                pnl_big_cell(canvas, x + column, y + row, ink, pnl_pixel(rows, narrow, column, row * 2),
                             pnl_pixel(rows, narrow, column, row * 2 + 1));
        }
    }
}

int pnl_canvas_big(pnl_canvas_t *canvas, int x, int y, int size, int style, int gradient, const char *text)
{
    int width = pnl_big_width(size, text);
    int start = 0;
    const char *p;

    if (canvas == NULL || width == 0)
        return x;
    if (canvas->screen->linear)
        return pnl_canvas_text(canvas, x, y, style, text);
    for (p = text; *p != '\0'; p++) {
        int c = pnl_glyph_rows((unsigned char)*p) != NULL ? (unsigned char)*p : '?';

        pnl_big_glyph(canvas, x + start, y, size, style, gradient, start, width, c);
        start += (pnl_glyph_columns(c) + 1) * pnl_big_scale(size);
    }
    return x + width;
}

static int pnl_art_size(const char *art, int *out_width)
{
    int lines = 1;
    int width = 0;
    const char *line = art;

    while (line != NULL) {
        const char *end = (strchr)(line, '\n');
        size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        int w = pnl_text_width_n(line, len);

        width = w > width ? w : width;
        line = end != NULL ? end + 1 : NULL;
        lines += end != NULL;
    }
    *out_width = width;
    return lines;
}

int pnl_canvas_art(pnl_canvas_t *canvas, int x, int y, int style, int gradient, const char *art)
{
    int width = 0;
    int lines;
    int row;
    const char *line = art;

    if (canvas == NULL || art == NULL)
        return 0;
    lines = pnl_art_size(art, &width);
    for (row = 0; row < lines && line != NULL; row++) {
        const char *end = (strchr)(line, '\n');
        size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        size_t at = 0;
        int column = 0;

        while (at < len) {
            char glyph[8];
            size_t n = 1;

            while (at + n < len && ((unsigned char)line[at + n] & 0xc0) == 0x80 && n < sizeof(glyph) - 1)
                n++;
            memcpy(glyph, line + at, n);
            glyph[n] = '\0';
            if (glyph[0] != ' ')
                pnl_canvas_text(canvas, x + column, y + row, pnl_big_ink(style, gradient, column, width), glyph);
            column += pnl_text_width(glyph);
            at += n;
        }
        line = end != NULL ? end + 1 : NULL;
    }
    return lines;
}

int pnl_app_set_banner(pnl_app_t *app, const char *art, int gradient)
{
    char *copy = NULL;

    if (app == NULL)
        return PNL_ERR_ARG;
    if (art != NULL && (copy = pnl_mem_copy(&app->mem, art)) == NULL)
        return PNL_ERR_NOMEM;
    pnl_mem_free(&app->mem, app->banner);
    app->banner = copy;
    app->banner_gradient = gradient;
    app->banner_set = 1;
    app->dirty = 1;
    return PNL_OK;
}

int pnl_banner_draw(pnl_app_t *app, pnl_canvas_t *canvas, int needed)
{
    int style = PNL_BOLD | PNL_FG(PNL_ROLE_ACCENT);
    int width = 0;
    int height;

    if (!app->banner_set || canvas->screen->linear)
        return 0;
    height = app->banner != NULL ? pnl_art_size(app->banner, &width) : pnl_big_height(PNL_BIG_SMALL);
    if (app->banner == NULL)
        width = pnl_big_width(PNL_BIG_SMALL, app->title);
    if (width + 2 > pnl_canvas_width(canvas) || height + needed + PNL_BANNER_ROOM / 4 > pnl_canvas_height(canvas))
        return 0;
    if (app->banner != NULL)
        pnl_canvas_art(canvas, 2, 0, style, app->banner_gradient, app->banner);
    else
        pnl_canvas_big(canvas, 2, 0, PNL_BIG_SMALL, style, app->banner_gradient, app->title);
    return height + 1;
}

static void pnl_big_widget_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_big_widget *big = ctx;

    (void)app;
    pnl_canvas_big(canvas, 0, 0, big->size, big->style, big->gradient, big->text);
}

static int pnl_big_widget_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_big_widget *big = ctx;

    (void)width;
    return app->screen.linear ? 1 : pnl_big_height(big->size);
}

static void pnl_art_widget_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_big_widget *art = ctx;

    (void)app;
    pnl_canvas_art(canvas, 0, 0, art->style, art->gradient, art->text);
}

static int pnl_art_widget_height(void *ctx, pnl_app_t *app, int width)
{
    struct pnl_big_widget *art = ctx;
    int art_width = 0;

    (void)app;
    (void)width;
    return pnl_art_size(art->text, &art_width);
}

static void pnl_big_widget_release(void *ctx)
{
    struct pnl_big_widget *big = ctx;

    pnl_mem_free(&big->app->mem, big->text);
    pnl_mem_free(&big->app->mem, big);
}

static int pnl_big_make(pnl_app_t *app, int kind, const pnl_widget_class_t *widget_class, const char *text,
                        int size, int style, int gradient, pnl_widget_t **out_widget)
{
    struct pnl_big_widget *big = pnl_mem_alloc(&app->mem, sizeof(*big));

    if (big == NULL)
        return PNL_ERR_NOMEM;
    memset(big, 0, sizeof(*big));
    big->app = app;
    big->size = size;
    big->style = style;
    big->gradient = gradient;
    if (pnl_widget_copy(app, &big->text, text) != PNL_OK) {
        pnl_big_widget_release(big);
        return PNL_ERR_NOMEM;
    }
    return pnl_widget_builtin(app, kind, widget_class, big, out_widget);
}

int pnl_widget_big(pnl_app_t *app, const char *text, int size, int style, int gradient, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t big_class = {pnl_big_widget_draw, NULL, pnl_big_widget_height, NULL,
                                                 pnl_big_widget_release};

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || text == NULL || out_widget == NULL || (size != PNL_BIG_SMALL && size != PNL_BIG_LARGE))
        return PNL_ERR_ARG;
    return pnl_big_make(app, PNL_WIDGET_BIG, &big_class, text, size, style, gradient, out_widget);
}

int pnl_widget_art(pnl_app_t *app, const char *art, int style, int gradient, pnl_widget_t **out_widget)
{
    static const pnl_widget_class_t art_class = {pnl_art_widget_draw, NULL, pnl_art_widget_height, NULL,
                                                 pnl_big_widget_release};

    if (out_widget != NULL)
        *out_widget = NULL;
    if (app == NULL || art == NULL || out_widget == NULL)
        return PNL_ERR_ARG;
    return pnl_big_make(app, PNL_WIDGET_ART, &art_class, art, 0, style, gradient, out_widget);
}
