#!/usr/bin/env bash
set -euo pipefail

cd /home/xh/ClusterChatServer
source admin_service/.venv/bin/activate

uvicorn admin_service.app.main:app --host 127.0.0.1 --port 8010 >/tmp/admin_service_uvicorn.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT

sleep 3
curl -sf http://127.0.0.1:8010/metrics | rg '^chat_'
