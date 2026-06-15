#include "pnl_internal.h"

#include <string.h>

#define PNL_KEY_CTRL_A 0x01
#define PNL_KEY_CTRL_E 0x05
#define PNL_KEY_CTRL_U 0x15
#define PNL_KEY_CTRL_W 0x17

void pnl_edit_init(struct pnl_edit *edit, char *text, size_t cap)
{
    edit->text = text;
    edit->cap = cap;
    edit->len = strlen(text);
    edit->cursor = edit->len;
}

static size_t pnl_edit_prev(const struct pnl_edit *edit, size_t pos)
{
    while (pos > 0 && ((unsigned char)edit->text[--pos] & 0xc0) == 0x80)
        ;
    return pos;
}

static size_t pnl_edit_next(const struct pnl_edit *edit, size_t pos)
{
    while (pos < edit->len && ((unsigned char)edit->text[++pos] & 0xc0) == 0x80)
        ;
    return pos;
}

static void pnl_edit_cut(struct pnl_edit *edit, size_t from, size_t to)
{
    memmove(edit->text + from, edit->text + to, edit->len - to + 1);
    edit->len -= to - from;
    edit->cursor = from;
}

static int pnl_edit_insert(struct pnl_edit *edit, int cp)
{
    char bytes[4];
    size_t n = pnl_utf8_put((uint32_t)cp, bytes);

    if (edit->len + n >= edit->cap)
        return 1;
    memmove(edit->text + edit->cursor + n, edit->text + edit->cursor, edit->len - edit->cursor + 1);
    memcpy(edit->text + edit->cursor, bytes, n);
    edit->len += n;
    edit->cursor += n;
    return 1;
}

static size_t pnl_edit_word_start(const struct pnl_edit *edit)
{
    size_t pos = edit->cursor;

    while (pos > 0 && edit->text[pos - 1] == ' ')
        pos--;
    while (pos > 0 && edit->text[pos - 1] != ' ')
        pos--;
    return pos;
}

int pnl_edit_key(struct pnl_edit *edit, int key)
{
    if (key == PNL_KEY_LEFT)
        edit->cursor = pnl_edit_prev(edit, edit->cursor);
    else if (key == PNL_KEY_RIGHT)
        edit->cursor = pnl_edit_next(edit, edit->cursor);
    else if (key == PNL_KEY_HOME || key == PNL_KEY_CTRL_A)
        edit->cursor = 0;
    else if (key == PNL_KEY_END || key == PNL_KEY_CTRL_E)
        edit->cursor = edit->len;
    else if (key == PNL_KEY_BACKSPACE && edit->cursor > 0)
        pnl_edit_cut(edit, pnl_edit_prev(edit, edit->cursor), edit->cursor);
    else if (key == PNL_KEY_DELETE && edit->cursor < edit->len)
        pnl_edit_cut(edit, edit->cursor, pnl_edit_next(edit, edit->cursor));
    else if (key == PNL_KEY_CTRL_U)
        pnl_edit_cut(edit, 0, edit->cursor);
    else if (key == PNL_KEY_CTRL_W)
        pnl_edit_cut(edit, pnl_edit_word_start(edit), edit->cursor);
    else if (key >= 0x20 && key < 0x110000 && key != 0x7f)
        return pnl_edit_insert(edit, key);
    else
        return key == PNL_KEY_BACKSPACE || key == PNL_KEY_DELETE;
    return 1;
}

static int pnl_edit_width(const char *text, size_t len)
{
    size_t pos = 0;
    int width = 0;

    while (pos < len) {
        uint32_t cp;
        int w;

        pos += pnl_utf8_next(text + pos, len - pos, &cp);
        w = pnl_cp_width(cp);
        width += w < 0 ? 1 : w;
    }
    return width;
}

void pnl_edit_draw(const struct pnl_edit *edit, pnl_canvas_t *canvas, int x, int y, int width, int focused)
{
    pnl_edit_draw_styled(edit, canvas, x, y, width, focused, 0);
}

void pnl_edit_draw_styled(const struct pnl_edit *edit, pnl_canvas_t *canvas, int x, int y, int width, int focused,
                          int style)
{
    size_t start = 0;
    char cursor_cell[5] = " ";
    size_t cell_len;
    int cursor_x;

    if (width < 2)
        return;
    while (pnl_edit_width(edit->text + start, edit->cursor - start) > width - 1)
        start = pnl_edit_next(edit, start);
    cursor_x = x + pnl_edit_width(edit->text + start, edit->cursor - start);
    pnl_canvas_text(canvas, x, y, style, edit->text + start);
    if (!focused)
        return;
    cell_len = pnl_edit_next(edit, edit->cursor) - edit->cursor;
    if (cell_len > 0 && cell_len < sizeof(cursor_cell)) {
        memcpy(cursor_cell, edit->text + edit->cursor, cell_len);
        cursor_cell[cell_len] = '\0';
    }
    pnl_canvas_text(canvas, cursor_x, y, style | PNL_REVERSE, cursor_cell);
}
