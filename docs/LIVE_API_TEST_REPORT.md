# Live API adapter validation — 5 November 2026

The integration is a later commit on `dev/spa-v062-demo-app`. V0.6.2 remains immutable at `3cc63df3fa445b5e01342122deafaa788749abcc`, with the original demo UI checkpoint at `bb305ccd27b6ad94ca5db20c2dd0dcadaa89ee62`.

## Executed final results

| Group | Passed | Failed |
| --- | ---: | ---: |
| Original C++ scenario sources through live integration wrapper | 141 | 0 |
| New API/parser/asset cases, Main and Demo | 36 | 0 |
| UI/service/model tests, including original 33 | 40 | 0 |
| Original demo browser suite | 14 | 0 |
| Browser → native C++ API/controller integration | 8 | 0 |
| Frozen file SHA-256 checks | 44 files match | 0 changed |

The combined host and browser command `RUN_BROWSER=1 bash tools/test-live.sh` was executed successfully. C++ compilations use `-std=c++17 -Wall -Wextra -Werror`. The original scenario sources are unchanged; their live-wrapper executables and results are separate from frozen baseline evidence.

## ESP32 compilation

| Build | Result | Flash / limit | RAM / limit | Final warnings |
| --- | --- | --- | --- | --- |
| Main Live | PASS | 1011847 / 3145728 | 47480 / 327680 | 0 |
| Demo Live | PASS | 1011391 / 3145728 | 47496 / 327680 | 0 |


Both **new** entry sketches compiled with Arduino CLI 1.3.1, ESP32 core 3.3.2, Adafruit ADS1X15 2.5.0, Adafruit BusIO 1.17.4 and ArduinoJson 7.4.2. Same board settings as V0.6.2: `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc`; other defaults retained. Existing dependencies/build outputs were reused where available. New build evidence is under `firmware/live-logs/`; baseline logs are unchanged.

Warnings were enabled. The first full toolchain build repeated upstream HAL aggregate-initializer warnings; final cached builds did not repeat dependency warnings. No adapter-source warning remained. No hardware upload was performed.

## Coverage and findings

API tests execute the actual shared adapter/controller code with mocked Arduino/NVS/I2C/watchdog/socket interfaces. They test:

- Complete nullable sensor fields, explicit capability/build identity and no invented physical flow/heater permission.
- Basic authentication on API routes, root and every static asset.
- Target bounds/steps, wrong scalar types, malformed JSON, duplicate keys, trailing content and unexpected output fields.
- HIGH sequencing through the frozen arbiter, idempotent light state, persistent Service entry/exit, fault rejection and failed NVS save acknowledgement.
- Command rate limiting with shutdown exempt, exact whole-hour daily schedule translation, and rejection of unsupported Normal/minute/weekday requests.
- Byte-exact streamed gzip assets, retained TX budget, Origin rejection for cross-site legacy requests, bounded history and service-only observations.

The eight browser/native checks load gzip assets from the actual C++ adapter via a test-only HTTP/stdio gateway. They exercise capability-aware live controls, saved target requests, delayed HIGH changeover, supported schedule editing, actual controller Service persistence across page reload, sensor-fault capture and acknowledgement, and transport-loss recovery. Sensor injection exists only in this host-test bridge, not a firmware endpoint.

This test exposed two integration defects that isolated mocks missed: unbound browser `fetch` and JSON fields lost through a nullable-value proxy. Both were fixed and the final suite rerun. Another UI test covers legacy fractional targets moving onto the supported 0.5°C grid. Service diagnostics display available converted resistance without inventing raw ADC samples.

Original browser checks cover four screens at 360/390/430/1280px, 12 scenarios, stale/offline/rejected requests, fault details, confirmations and browser-local demo service persistence. UI source remains below the 100 KiB raw budget; gzip flash assets are about 20 KiB. Browser runs used Playwright 1.62.1 with the available Chrome headless shell 134.0.6998.88. No physical Android-device or Safari acceptance is claimed.

## Remaining limits

- Tests validate software integration, not electrical certification, physical contact release, flow, sensor calibration, actual radio/TCP/flash latency or watchdog reset timing.
- Both physical heater output pins remain inhibited. Live Demo uses simulated sensor inputs but can command other relay channels; bench loads must be isolated.
- HTTP Basic is unencrypted; trusted LAN only. No internet exposure, cloud or Matter path was added.
- The adapter intentionally rejects unreviewed manual heating semantics and finer/week-specific scheduling. Core scheduling and safety logic are unchanged.
- Captured events are bounded RAM observations after baseline safety processing, not pre-trip forensic telemetry or persistent history. Wall-clock timestamps can be unavailable and are displayed as uptime.
- Raw ADC is not retained by the frozen baseline. No GPIO, SSR or contactor testing endpoint is provided.

Use `LIVE_API_INTEGRATION.md` for build/run instructions and `BASELINE_V062.md` for isolated rollback. Physical commissioning remains outstanding.
