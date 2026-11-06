#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc'
python3 live-tests/verify_frozen.py
mkdir -p topside-logs
config_args=()
if [[ -n "${ARDUINO_CONFIG_FILE:-}" ]]; then config_args=(--config-file "$ARDUINO_CONFIG_FILE"); fi
for variant in Main Demo; do
    name="${variant,,}"
    "$CLI" compile "${config_args[@]}" -b "$FQBN" --library "$PWD/libraries/SpaController" --library "$PWD/libraries/SpaLiveApi" --library "$PWD/libraries/SpaTopside" --warnings all --build-path "${SPA_LIVE_BUILD_ROOT:-$PWD/.build-live}-$name" "Spa${variant}_Live" > "topside-logs/build-$name.log" 2>&1
    printf '%s live build PASS\n' "$variant"
done
