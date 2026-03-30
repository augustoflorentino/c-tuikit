#include "pnl_internal.h"

static int pnl_keys_letter(unsigned char c)
{
    static const int arrows[] = {PNL_KEY_UP, PNL_KEY_DOWN, PNL_KEY_RIGHT, PNL_KEY_LEFT};

    if (c >= 'A' && c <= 'D')
        return arrows[c - 'A'];
    if (c == 'H')
        return PNL_KEY_HOME;
    if (c == 'F')
        return PNL_KEY_END;
    return PNL_KEY_NONE;
}

static int pnl_keys_tilde(const unsigned char *data, size_t end)
{
    static const int codes[] = {PNL_KEY_NONE, PNL_KEY_HOME, PNL_KEY_NONE, PNL_KEY_DELETE, PNL_KEY_END,
                                PNL_KEY_PAGE_UP, PNL_KEY_PAGE_DOWN, PNL_KEY_HOME, PNL_KEY_END};

    if (end != 3 || data[2] < '1' || data[2] > '8')
        return PNL_KEY_NONE;
    return codes[data[2] - '0'];
}

static size_t pnl_keys_csi(const unsigned char *data, size_t data_len, int *out_key)
{
    size_t i;

    for (i = 2; i < data_len; i++) {
        if (data[i] < 0x40 || data[i] > 0x7e)
            continue;
        if (i == 2 && data[i] == 'Z')
            *out_key = PNL_KEY_BACKTAB;
        else if (i == 2)
            *out_key = pnl_keys_letter(data[i]);
        else if (data[i] == '~')
            *out_key = pnl_keys_tilde(data, i);
        return i + 1;
    }
    return data_len;
}

static size_t pnl_keys_osc(const unsigned char *data, size_t data_len)
{
    size_t i;

    for (i = 2; i < data_len; i++) {
        if (data[i] == 0x07)
            return i + 1;
        if (data[i] == 0x1b && i + 1 < data_len && data[i + 1] == '\\')
            return i + 2;
    }
    return data_len;
}

static size_t pnl_keys_escape(const unsigned char *data, size_t data_len, int *out_key)
{
    if (data_len > 1 && data[1] == ']')
        return pnl_keys_osc(data, data_len);
    if (data_len == 1 || (data[1] != '[' && data[1] != 'O')) {
        *out_key = PNL_KEY_ESCAPE;
        return 1;
    }
    if (data[1] == '[')
        return pnl_keys_csi(data, data_len, out_key);
    if (data_len < 3)
        return data_len;
    *out_key = pnl_keys_letter(data[2]);
    return 3;
}

static size_t pnl_keys_utf8(const unsigned char *data, size_t data_len, int *out_key)
{
    size_t need = data[0] >= 0xf0 ? 4 : data[0] >= 0xe0 ? 3 : 2;
    int code = data[0] & (0x3f >> (need - 1));
    size_t i;

    if (data[0] < 0xc2 || data[0] > 0xf4)
        return 1;
    for (i = 1; i < need && i < data_len; i++) {
        if ((data[i] & 0xc0) != 0x80)
            return i;
        code = (code << 6) | (data[i] & 0x3f);
    }
    if (data_len < need)
        return data_len;
    if (code < 0x110000 && (code < 0xd800 || code > 0xdfff) && code >= (need == 3 ? 0x800 : need == 4 ? 0x10000 : 0))
        *out_key = code;
    return need;
}

size_t pnl_key_decode(const unsigned char *data, size_t data_len, int *out_key)
{
    if (out_key == NULL)
        return 0;
    *out_key = PNL_KEY_NONE;
    if (data == NULL || data_len == 0)
        return 0;
    if (data[0] == 0x1b)
        return pnl_keys_escape(data, data_len, out_key);
    if (data[0] == '\r' || data[0] == '\n') {
        *out_key = PNL_KEY_ENTER;
        return 1;
    }
    if (data[0] == '\t') {
        *out_key = PNL_KEY_TAB;
        return 1;
    }
    if (data[0] == 0x7f || data[0] == 0x08) {
        *out_key = PNL_KEY_BACKSPACE;
        return 1;
    }
    if (data[0] < 0x80) {
        *out_key = data[0];
        return 1;
    }
    return pnl_keys_utf8(data, data_len, out_key);
}
