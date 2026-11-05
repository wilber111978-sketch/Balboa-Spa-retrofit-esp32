#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc'
mkdir -p logs
for variant in Main Demo; do
    build_name="${variant,,}"
    "$CLI" compile -b "$FQBN" --library "$PWD/libraries/SpaController" --warnings all --build-path "${SPA_BUILD_ROOT:-$PWD/.build}-$build_name" "Spa${variant}_V0_6" > "logs/build-$build_name.log" 2>&1
    printf '%s build PASS\n' "$variant"
done
