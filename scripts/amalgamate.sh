#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$root/dist/c-tuikit/c-tuikit.h}
mkdir -p "$(dirname "$out")"

{
    cat "$root/include/c-tuikit/c-tuikit.h"
    echo
    echo "#if defined(PNL_IMPLEMENTATION) && !defined(PNL_IMPLEMENTATION_DONE)"
    echo "#define PNL_IMPLEMENTATION_DONE"
    for src in "$root"/src/*.h "$root"/src/*.c; do
        [ -f "$src" ] && grep -v '^#include "' "$src"
    done
    echo "#endif"
} > "$out"
echo "$out"
