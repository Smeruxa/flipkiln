#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
find src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 |
    xargs -0 clang-format --dry-run --Werror
find src tests migrations CMakeLists.txt -type f \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.sql' -o -name '*.py' -o -name 'CMakeLists.txt' \) \
    -exec awk 'length($0) > 110 {
        printf "%s:%d: line length %d exceeds 110\n", FILENAME, FNR, length($0)
        failing = 1
    } END {exit failing}' {} +
ruff check tests
ruff format --check tests
