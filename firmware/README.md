# Spa Controller V0.6.2 — service and recovery safeguards

Main and Demo use `libraries/SpaController/src/SpaController.h`. Both keep physical heater outputs CH1/CH3 OFF. Main's commissioning flag remains false; Demo models heater activity in RAM. CH2/CH4/CH5/CH6 can operate outside Service Mode. Use isolated bench loads; this is not a live-heater release.

## Shutdown commands

| Command | Meaning | Restart behavior |
| --- | --- | --- |
| `off` | Stop jets/manual pump demand. Heating demand, filtration and after-use may request LOW again. Light stays independent. | Manual pump requests do not resume. |
| `heatmode off` | Disable heating demand; established filtration/after-use and any postheat run-on remain. | Saved OFF mode retained. |
| `alloff` | Enter Service Mode immediately: all six outputs OFF; automatic operation blocked. | Service remains ON after restart. |
| `service on` | Same shutdown as `alloff`. | Service remains ON after restart. |
| `service off confirm` | Exit Service Mode only if settings and fault state are healthy. Clears manual/Ready requests; established automatic scheduling can resume. | Exited state saved and verified before acknowledgement. |
| `resetfault` | Acquire fresh measurements and acknowledge a cleared cause. Does not exit Service Mode. | Verified cleared fault saved; failed save returns an error and remains stopped. |

Service Mode pauses the after-use remainder, blocks LOW/HIGH/heat/Ready/after-use/light commands and overrides automatic demand. Settings edits remain available. It is a software inhibit, not electrical isolation or an emergency-stop certification. A failed persistence operation stops outputs and reports an error; failed storage cannot guarantee retention of a newly requested inhibit across power loss.

Main can now acknowledge recovered sensor/pump faults without an unassigned high-limit input. An assigned physical high-limit still must be healthy. This does not commission heating: Main heater permission and the unconditional CH1/CH3 pin gate remain closed. Configuration/ADC initialization faults require correction and restart before acknowledgement. Integrity faults require deliberate repair of the stored record/NVS; there is no unauthenticated or automatic reset-to-defaults command.

## Safeguards

- ESP-IDF task-watchdog user watches complete control-loop progress with a 5-second timeout and panic/reset enabled. It is fed only after the loop completes. Watchdog/panic/brownout restart reasons latch a fault and persistent Service Mode before operation.
- Sensor freshness limit is 2 seconds. Old timestamps are checked before commands/network work and again before acquisition can replace them. Stale readings latch a fault and remove heater/pump/UV outputs.
- Configuration, service/fault state and runtime are independent schema-versioned CRC32 NVS blobs. Complete blobs are written and read back before success; semantic range checks remain. Presence markers prevent silent legacy fallback when a migrated record goes missing. Valid legacy settings are migrated once. CRC detects accidental corruption; it is not a security mechanism.
- HTTP has one active connection, 128-byte RX and TX budgets per loop call, 1,024-byte headers, 384-byte form bodies, 96-character decoded commands, 4,096-byte response bodies and separate absolute 1.5-second receive/transmit deadlines. Socket IO uses `MSG_DONTWAIT`; no blocking HTTP stream reads, flushes, uploads, chunking or keepalive. A callback runs at most one command per completed request. Kernel, flash and Wi-Fi driver timings still require real-board measurement.

## Relay map — unchanged

CH1 GPIO1 safety contactor; CH2 GPIO2 LOW; CH3 GPIO41 SSR control; CH4 GPIO42 HIGH; CH5 GPIO45 ozone/UV; CH6 GPIO46 light. Physical CH1/CH3 are inhibited in both builds.

## Reproduce tests and builds

Pinned dependencies: Arduino CLI 1.3.1, ESP32 core 3.3.2, Adafruit ADS1X15 2.5.0, Adafruit BusIO 1.17.4. Host compiler: g++ with C++17.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.2 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install 'Adafruit ADS1X15@2.5.0' 'Adafruit BusIO@1.17.4'
python3 tests/run_all.py
bash build.sh
```

`ARDUINO_CLI` can name a CLI executable. `SPA_BUILD_ROOT` sets a reusable build-directory prefix. Scripts never flash hardware. Tests execute the actual shared firmware, persistence and HTTP parser using mock peripherals/watchdog/NVS/socket interfaces. They do not execute ESP32 machine code or physically test reset timing, relays, radio or mains protections.

FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc`. Other board menu defaults remain unchanged. Verify the actual fitted module. For Arduino IDE, copy `libraries/SpaController` into the sketchbook libraries folder, install the pinned dependencies and open either sketch. Keep the shared library with the sketches.

ADS1115: 0x48, AIN0/AIN1, GAIN_ONE, SDA GPIO8/SCL GPIO9, assumed 3.3 V / 30 kΩ divider model. Verify actual probes and wiring. Wi-Fi is disabled until `SPA_WIFI_SSID`, `SPA_WIFI_PASS` and a unique `SPA_WEB_PASS` are configured. HTTP Basic is plaintext; use a trusted isolated LAN.

## Scheduling and other controls — unchanged

Default solar window 11:00–16:00, inclusive/exclusive endpoints; overnight windows supported and equal endpoints closed. Four-hour commanded LOW quota, preferred 11:00–13:00 and 15:00–16:00, catch-up within the selected window. Ready/Standby hysteresis 1°C/3°C; Ready timeout 2 hours. HIGH timeout 20 minutes; after-use 60 accumulated LOW minutes with ozone/UV, paused by HIGH/faults/Service Mode. Existing cadence and thresholds remain unchanged.

Commands: `low`, `high`, `off`, `alloff`, `service on`, `service off confirm`, `heatmode off|solar|manual`, `window 11-16`, `setpoint 38`, `ready`, `standby`, `light`, `afteruse`, `filterstatus`, `filtercleaned confirm`, `resetfault`, `status`, `events`. `heat` remains a LOW alias. Demo injections remain Demo-only.

Runtime checkpoint interval remains 15 minutes, with immediate snapshots on jets entry/exit and explicit after-use requests. Power loss may lose runtime credit or replay a conservative after-use remainder. Manual heat/LOW/HIGH/Ready do not restart automatically. Solar/after-use can resume outside Service Mode after a healthy boot. Filter-clean calendar age is unknown until a valid clean date is acknowledged.

See `reports/TEST_REPORT.md` and `reports/COMMISSIONING_CHECKLIST.md`. No live-heater enablement, APK or unrelated integrations are included.
