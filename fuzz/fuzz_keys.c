#include "c-tuikit/c-tuikit.h"

#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    while (size > 0) {
        int key;
        size_t used = pnl_key_decode(data, size, &key);

        if (used == 0 || used > size)
            __builtin_trap();
        data += used;
        size -= used;
    }
    return 0;
}
