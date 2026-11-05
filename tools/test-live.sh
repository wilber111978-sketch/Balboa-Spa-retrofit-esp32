#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/embed-assets.py
python3 firmware/live-tests/verify_frozen.py
python3 firmware/live-tests/run_live.py
python3 firmware/live-tests/run_regressions.py
python3 firmware/live-tests/build_bridge.py
npm test
npm run size
if [[ "${RUN_BROWSER:-0}" == "1" ]]; then
    mkdir -p artifacts
    python3 -m http.server 8080 --bind 127.0.0.1 --directory firmware/data > artifacts/static-server.log 2>&1 &
    spa_server_pid=$!
    trap 'kill "$spa_server_pid" 2>/dev/null || true' EXIT
    SCREENSHOT_DIR=artifacts/demo-screenshots npm run test:browser
    npm run test:live-browser
fi
