#!/usr/bin/env bash
set -euo pipefail
CLI="${ARDUINO_CLI:-arduino-cli}"
args=()
if [[ -n "${ARDUINO_CONFIG_FILE:-}" ]]; then args=(--config-file "$ARDUINO_CONFIG_FILE"); fi
"$CLI" core update-index "${args[@]}" --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
"$CLI" core install esp32:esp32@3.3.2 "${args[@]}" --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
"$CLI" lib install 'Adafruit ADS1X15@2.5.0' 'ArduinoJson@7.4.2' 'Adafruit GC9A01A@1.1.1' 'Adafruit GFX Library@1.12.1' 'Adafruit BusIO@1.17.4' "${args[@]}"
