# Live API integration — V0.6.3 adapter, API v1

## Structure and baseline

[The V0.6.2 checkpoint is frozen](BASELINE_V062.md). Its three control-library headers, two original sketches, test sources and evidence remain byte-identical. New code is under `firmware/libraries/SpaLiveApi/`; new entry sketches are `SpaMain_Live` and `SpaDemo_Live`.

The adapter preincludes the original bounded-server interface, substitutes a compatible JSON/asset server type and renames the original `setup`/`loop` entry points while including the frozen shared controller. Its own entry points call those baseline functions. It does not fork or replace sensor processing, pump sequencing, NVS records, scheduling, watchdog behavior or relay writes.

**Main and Demo both retain the unconditional physical CH1/CH3 inhibit.** Demo live firmware uses simulated sensor inputs while other output commands can operate real relay channels. Its UI displays this distinction. No Matter, panel decoding, raw output tests or mains commissioning is added.

## Build and run on a controller

Install the baseline's pinned ESP32 core 3.3.2, Adafruit ADS1X15 2.5.0 and BusIO 1.17.4, plus **ArduinoJson 7.4.2**:

```sh
arduino-cli lib install 'ArduinoJson@7.4.2'
cd firmware
bash build-live.sh
```

The script first verifies the frozen-file checksums, generates `WebAssets.h` from `firmware/data/` and compiles both new sketches with the same board settings as V0.6.2. `ARDUINO_CLI`, `ARDUINO_CONFIG_FILE` and `SPA_LIVE_BUILD_ROOT` optionally select the CLI/config/cache paths. The original `build.sh` still builds the original baseline sketches.

For Arduino IDE, generate assets with `python3 tools/embed-assets.py` from the repository root, install ArduinoJson, copy both `SpaController` and `SpaLiveApi` into the sketchbook libraries folder and open the selected **new** live sketch.

Configure the baseline's existing `SPA_WIFI_SSID`, `SPA_WIFI_PASS` and unique `SPA_WEB_PASS` definitions before a network-enabled bench build, using the same configuration mechanism as V0.6.2. The unconfigured placeholder still disables network startup; no open AP fallback is added. Do not commit real credentials.

After an intentionally configured, isolated bench flash, browse to `http://<controller-LAN-address>/`. HTTP Basic authentication is required for the UI, assets and APIs. Credentials are handled by the browser, not stored by the app. HTTP is unencrypted: use a trusted isolated LAN and no internet port forwarding. Firmware flashing and physical acceptance were not performed in this task.

Static resources are deterministically gzip-embedded into flash and streamed in 128-byte TX chunks. No separate filesystem image/upload is needed. `runtime.js` is supplied dynamically by the controller and selects `HttpSpaService`. A plain local static server still serves the checked-in demo `runtime.js` and selects `DemoSpaService`.

## Implemented API

- Authenticated `GET /api/capabilities`, `/api/state`, `/api/faults`, `/api/diagnostics`.
- Authenticated JSON `POST /api/target`, `/api/jets`, `/api/light`, `/api/mode`, `/api/schedule`, `/api/service-mode`, `/api/fault-reset`.
- Command success is `{"accepted":true,"message":"..."}` only after the baseline command returns OK. The UI then rereads controller state; requested jets speed and actual speed remain separate during changeover.
- Light requests are absolute/idempotent. A repeated `{"on":true}` does not toggle the light off.
- Fault, Service Mode and settings failures reject ordinary starts. Explicit service entry, jets OFF and heating OFF are not blocked by the ordinary 250 ms command throttle. Service exit/acknowledgement still require the existing controller checks.
- JSON bodies are limited to 384 bytes and shallow nesting; malformed objects, duplicate/escaped field names, trailing content, unknown fields and wrong scalar types are rejected. Cross-origin POSTs are rejected when an Origin header differs from Host. No GPIO/relay/SSR endpoint exists.
- Legacy `/api/status`, `/api/events` and form `/api/cmd` remain for backward compatibility and retain the baseline command behavior. The new UI does not use them.

The receive/transmit budgets and 1.5-second phase deadlines remain bounded. Streaming avoids copying an entire asset into the response buffer. Real stack/driver/flash latency is not a proven hard real-time bound.

## Capabilities and deliberate limits

The live UI uses the actual controller capability response:

- Target UI range 20–40°C, 0.5°C grid. Existing fractional baseline targets remain readable and +/- moves to the next supported grid point.
- Heating modes accepted by this adapter are **OFF** and **SOLAR_PRIORITY**. `NORMAL` is rejected rather than silently becoming indefinite manual heating. A legacy serial/manual mode is reported as `MANUAL_LEGACY` without granting the app a manual-override command.
- Schedule windows retain whole-hour precision and apply every day. Minute/weekday requests are rejected, never rounded or dropped. Equal endpoints disable the window; the UI derives the enable flag from that existing rule. Overnight windows retain baseline semantics.
- Heater physical permission remains false. Logical Demo heater commands are not claimed to be physical energisation. Flow is always reported as unproven (`flow_ok:false`, CHECK without a fault); a running pump relay is not flow feedback.
- Raw ADC counts are not retained by V0.6.2 and are returned as null. Converted resistance/permissive observations are exposed only in Service Mode; no direct hardware controls are provided.
- Fault/event history is an eight-record RAM ring. Responses return newest records that fit the 3.5 KB JSON budget; it is not persistent history or an unlimited archive.
- Captures are observations immediately after baseline safety processing, not claimed pre-trip telemetry. OEM-style panel labels are diagnostic approximations; the numeric internal fault ID is retained. Where a wall-clock event time was not captured, uptime is displayed instead of a fabricated date.

## Repeatable tests

For host C++ integration tests, point `ARDUINOJSON_SRC` to the installed ArduinoJson 7.4.2 `src` directory if it is not found in the usual sketchbook path:

```sh
python3 firmware/live-tests/verify_frozen.py
python3 firmware/live-tests/run_live.py
python3 firmware/live-tests/run_regressions.py
npm test
npm run size
```

Native bridge + actual-browser integration (requires g++, Node and Playwright):

```sh
python3 firmware/live-tests/build_bridge.py
npm install
npx playwright install chromium
npm run test:live-browser
```

The bridge/gateway are **host-test-only files**, not deployed on the controller. They run the actual C++ HTTP adapter and controller with mocked peripherals/NVS/socket interfaces; test-only sensor injection is not a firmware API. Browser requests traverse real HTTP to this driver. See `LIVE_API_TEST_REPORT.md` for executed results and limits.

Use the existing `npm run test:browser` against a plain static server for the unchanged demo workflow. Hardware Wi-Fi, TCP latency, actual NVS atomicity, motor contactors, independent thermal/flow safeguards and watchdog reset timing remain acceptance work. Software API/UI tests do not certify electrical safety.

Combined host checks: `bash tools/test-live.sh`; add `RUN_BROWSER=1` after installing Playwright/Chromium to run both browser suites as well. Browser screenshots from this combined command go under ignored `artifacts/`, preserving the original demo screenshots.
