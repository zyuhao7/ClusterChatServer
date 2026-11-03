#!/usr/bin/env bash
set -euo pipefail

if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck not installed. Install it with: sudo apt install cppcheck" >&2
    exit 127
fi

cd "$(dirname "$0")/.."

cppcheck \
    --enable=warning,style,performance,portability \
    --std=c++11 \
    --language=c++ \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --suppress=internalAstError:thirdparty/json.hpp \
    --error-exitcode=1 \
    -I include \
    -I include/server \
    -I thirdparty \
    src include
