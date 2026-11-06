# V0.8 implementation and verification report

Completed software implementation:6 November2026. Branch:`dev/spa-v062-demo-app`; inspected starting HEAD:`e17fdec0e59cce09cd1963c5e7c3273363c03028`. This report is committed alongside the implementation; use its Git history for the resulting commit identifier.

## Delivered

- Shared `SpaTopside` library: four debounced buttons, short/long/repeat/stuck handling, Normal/Adjustment/Fault/Service/information views and isolated GC9A01 worker.
- Updated existing Main/Demo live entry points. Existing commands, API, web dashboard, hourly scheduling, circulation, persistence and fault logic reused. No VL260 protocol code included.
- Main GPIO input/display worker; Demo no-hardware mode and compile-time-only C++ input simulation hook. No production mock HTTP/serial route.
- Explicit physical heating inhibition, existing numeric faults/API diagnostic identifiers and rejection feedback. No direct relay access, automatic reset or protection-disabling gesture.
- Repeatable host tests, six SVG screen mocks and build/evidence files in`firmware/topside-logs/`.
- All44 frozen V0.6.2 files checksum-match the reviewed baseline. Relay assignments and commissioning locks are unchanged. Dashboard files and embedded assets are unchanged.

## Executed results

| Suite | Result | Evidence |
|---|---|---|
|Existing controller regressions through SpaTopside|141/141 PASS, includes original57|`firmware/topside-logs/regression-results.json`|
|Existing JSON/API cases through SpaTopside|36/36 PASS|`firmware/topside-logs/api-results.json`|
|Existing UI/model/service tests|40/40 PASS|Node test runner; unchanged sources|
|Existing browser checks|14 Demo +8 native C++ live PASS|`browser-results.json`, `live-browser-results.json` in topside logs|
|New topside scenarios|42/42 PASS:21 in each build|`firmware/topside-logs/results.json`|
|Frozen baseline|44/44 checksums match|`firmware/live-tests/verify_frozen.py`|
|Assets|57948 raw /20132 gzip bytes, PASS|Existing asset budget test|

Total281 executed software cases/checks, plus44 frozen-file comparisons. No remaining software-test failures. The complete existing suites ran once; after a final five-second interaction-timer refinement only the affected topside suite and incremental firmware builds were repeated. Successful unrelated full suites were not rerun.

New case expectations cover startup/no display,40ms bounce rejection,800ms Light information gesture without toggling,250ms bounded temperature repeat/no catch-up,20–40C limits/0.5C steps, five-second preview/interaction extension, Jets during logical heating, Service rejection, all22 existing fault values overriding controls/screens, nonfinite/stale readings, boot-held/stuck suppression, offline Wi-Fi operation, shared API/panel limiter, simultaneous buttons, disconnected mock/reconnected sink, watchdog/brownout restart lockout, a300ms blocked renderer while actual core fault handling progresses, failed settings persistence and unchanged heat mode/window. Tests use actual controller functions with existing mocked NVS/sensors/time/sockets. None constitute a real electrical test.

An initial targeted run failed two stale-display expectations. That exposed reliance on a mutating validation helper; the adapter was corrected to read status/timestamps without altering faults. Both variants then passed. No failing test was reported as a pass.

## Builds

| Setting | Tested configuration |
|---|---|
|CLI|Arduino CLI1.3.1|
|Core|ESP32 Arduino3.3.2|
|Board|ESP32-S3,16MB flash,app3M_fat9M_16MB,PSRAM disabled,USB CDC on boot|
|FQBN|`esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc`|
|Graphics|Adafruit GC9A01A1.1.1; Adafruit GFX1.12.1|
|Other libraries|BusIO1.17.4; ADS1X15 2.5.0; ArduinoJson7.4.2|
|Main|PASS;1029239 flash bytes;47960 static RAM bytes|
|Demo|PASS;1013071 flash bytes;47544 static RAM bytes|

Compiler flag:`--warnings all`. Initial full compilation emitted38 vendor missing-field-initializer diagnostics per build in ESP-IDF SPI HAL headers (`gpspi_flash_ll.h`/`spimem_flash_ll.h`), also seen in the existing dependency set. No application warning/error was found. The final incremental build logs contain no warnings; unchanged SDK compilation was reused. No warning suppression or frozen-controller edit was introduced. Worker/strip runtime allocations are additional to compiler static RAM totals and need physical headroom testing.

Browser execution reused Playwright1.57.0 with Chrome headless134.0.6998.165; no frontend dependency files were changed. SVG previews were visually inspected at240x240 using the shared strip layout; host glyphs approximate the GFX font. Binary firmware was compiled, not flashed. Source hashes and exact build summaries are in`firmware/topside-logs/build-results.json`.

## Reproduce

```sh
# With Arduino CLI1.3.1 available; optional ARDUINO_CLI/ARDUINO_CONFIG_FILE overrides:
bash tools/install-topside-deps.sh
export ARDUINOJSON_SRC=/absolute/path/to/ArduinoJson/src
bash tools/test-topside.sh
bash firmware/build-live.sh
```

Optional browser checks follow`firmware/topside-tests/README.md`. No dashboard regeneration is required. Standard Main enables GPIO buttons/display task; Demo disables all topside hardware by default. Backlight remains LOW until the external driver is verified and`SPA_TOPSIDE_BACKLIGHT_VERIFIED=1` is explicitly selected. Neither flag relaxes heater lockouts.

## Remaining issues and physical acceptance

1. Actual module, display/backlight current, driver circuit, button/LED wiring, header continuity, cable signal quality and waterproof assembly are unverified. GPIO15 is an external driver-enable signal, never a backlight supply.
2. Write-only SPI has no reliable display-presence/health feedback. A cable disconnect or frozen image cannot be detected as a controller fault. Host disconnected-sink tests prove control independence only; real hot reconnect or supply-loss recovery may require reinitialization and remains untested.
3. Main's real FreeRTOS scheduling, initialization delays, SPI error/stall timing, task/heap headroom and sensor/fault output-removal latency need scope measurements. The blocked-renderer host test cannot establish a worst-case timing bound on hardware.
4. Backlight off-by-default can make a working display appear blank until the driver gate is deliberately configured. Do not bypass this by powering LEDs from GPIO15.
5. No mains commissioning or physical heater/pump safety validation was performed. Independent high-limit/flow/contactor/SSR checks, hardwired LOW/HIGH interlocking and qualified electrical inspection remain required.

Bench order:disconnect mains and all relay loads → verify module/supply/GPIO/backlight interface → scope reset/CS/backlight off states → test display and buttons on isolated low voltage → exercise faults/Service/Wi-Fi/slow rendering while monitoring all six outputs → repeat restart/brownout/watchdog tests and record measurements. Full detailed procedure:`docs/TOPSIDE_V08_HARDWARE.md`. Any electrical mismatch stops the test. Compilation and simulation do not establish physical compatibility or electrical safety certification.
