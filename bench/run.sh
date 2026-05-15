#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/tuikit_bench
${CC:-cc} -std=c99 -O2 -DNDEBUG -I"$root/include" -I"$root/src" "$root/bench/bench.c" "$root"/src/*.c -o "$out" -pthread
TUIKIT_MEMORY=off "$out"
