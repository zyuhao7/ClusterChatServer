#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if command -v ruff >/dev/null 2>&1; then
    RUFF=(ruff)
else
    RUFF=(uvx ruff)
fi

"${RUFF[@]}" check --config ruff.toml admin_service scripts tests
