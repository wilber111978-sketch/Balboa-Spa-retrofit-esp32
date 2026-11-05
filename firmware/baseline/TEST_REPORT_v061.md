# Spa Controller V0.6.1 — pre-flash audit and automated test report

Review date: 5 November 2026 (Sydney). Input: `Spa_V0_6_Development_Package.zip`, containing README and two 1,664-line sketches. Original sketches differed only in the Demo mode definition. Scope: full supplied firmware, shared control behavior, compilation, repeatable host simulations and commissioning review. No board or mains equipment was connected; nothing was flashed.

## Disposition

**Bench-validation candidate only. Not approved for live heating.** Both builds physically force CH1 and CH3 OFF. Main's commissioning flag remains false and its physical high-limit/qualification implementation remains unavailable. Demo tests logical heater behavior without energising either physical heater output.

Executed host scenarios: **57 passed, 0 failed** in the final run. ESP32 compilation results and memory figures are recorded below. Mock tests exercise the actual shared C++ implementation, not a separately rewritten controller model. Passing these tests or compiling does not establish electrical safety certification.

## Defects repaired

| Finding | Repair and evidence |
| --- | --- |
| Demo enabled physical heater outputs using simulated safety inputs | Unconditional CH1/CH3 gate in the shared relay writer; logical heater model retained. Every simulation step asserts both physical pins remain OFF. |
| After-use seconds assigned to a millisecond countdown | All requests now use 3,600,000 ms; full 60-minute simulated cycle executed, including LOW/UV and pause/resume. Original assignment produced 3.6 seconds. |
| HIGH pump could survive a non-lockout sensor fault | Fault handling immediately removes heater, both pump and UV outputs, cancels pending changeover and manual pump demand; pump processor also rejects any fault. |
| Heating and circulation used different clocks/window logic | Both use `spaClock`; Demo clock applies to heating; overnight and equal-endpoint behavior unified. |
| Faults disappeared on restart | First fault recorded in NVS after outputs are removed, restored as a lockout, and cleared only on successful acknowledgement. Fresh-process power-cut test executed. |
| Persistent manual heat could resume after a power cut | Saved manual mode becomes OFF at startup; solar mode can resume only with a valid clock and other gates. |
| ADC convenience read contains an unbounded conversion wait | Checked conversion has a 100 ms polling deadline plus configured 50 ms I2C transaction timeout and device ACK checks. ADC initialization/disconnection/stuck-conversion tests executed. These are not a measured whole-loop deadline. |
| Real overtemperature could be delayed by EMA filtering | Raw as well as filtered probe temperature now trips at 42°C. Nonfinite/stale samples fail closed. |
| Main/Demo sensor-range mismatch | Demo applies the same 0–60°C plausibility limits; invalid real samples reset slope history. |
| Corrupt NVS calibration/mode/window values accepted | Range/finite validation, safe defaults and a latched configuration fault; a separate validity flag prevents reset while the configuration remains invalid. |
| Malformed numeric commands accepted partial strings | Full numeric parsing for setpoint and Demo temperature; strict clock/window validation; command-length limits and serial overflow rejection. |
| Invalid sensor values produced invalid JSON | Status emits JSON `null`; output parsed by the test runner. |
| Unconfirmed filter-clean reset | Requires `filtercleaned confirm`; date saved only with a plausible epoch. |
| Runtime wrap/subsecond issues | Preserve subsecond LOW credit across pauses, saturate accumulated counters, avoid daily reset on backward clock correction, and use active/elapsed timing for postheat run-on. |
| Heater termination could drop circulation before run-on was recognised | LOW arbitration retains circulation while the heater model is still active and starts a 30-second run-on after it stops. |
| Startup left output initialization until after a delay | Relay OFF latches and output modes initialized before serial startup delay. Boot-ROM and hardware behavior still require measurement. |
| Duplicated Main/Demo implementation could drift | Both sketches include one shared library; only `SPA_BENCH_DEMO` differs. |
| Unreachable duplicate overtemperature branch / unused clear helper | Removed duplicate branch and unused recoverable-fault clearing helper. All faults now require acknowledgement. |

An early host run exposed the after-use unit defect. Other early failures also required correcting test preconditions (Ready versus Standby heat thresholds) and a persistence clamp that mixed seconds/milliseconds during repair. A first corrected Main compilation found an unsupported ADS library API; it was replaced with Wire ACK checks. Final executed results below supersede those intermediate candidates. The original input was inspected but was not separately certified as compiling.

## Build configuration and warnings

- Arduino CLI 1.3.1, ESP32 Arduino core 3.3.2.
- Adafruit ADS1X15 2.5.0; Adafruit BusIO 1.17.4.
- Core Wire, Preferences, WiFi, Networking, WebServer, FS, SPI and Hash libraries: bundled core version 3.3.2.
- FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc`.
- ESP32S3 Dev Module; 240 MHz; default QIO/80 MHz flash; 16 MB flash; 3 MB app partition; PSRAM disabled; hardware CDC/JTAG and USB CDC at boot. This targets the vendor's documented N16 module; actual fitted module/revision remains unverified.
- Compiler warnings enabled with `--warnings all`. Host tests use `g++ -std=c++17 -Wall -Wextra -Werror`.
- Build scripts reuse installed dependencies and build directories. No firmware upload or APK production was performed.

| Build | Result | Flash / limit | RAM / limit | Final log warnings |
| --- | --- | --- | --- | --- |
| Main | PASS (compiler exit 0) | 976323 / 3145728 bytes | 46984 / 327680 bytes | 0 |
| Demo | PASS (compiler exit 0) | 976103 / 3145728 bytes | 47000 / 327680 bytes | 0 |


The initial full build emitted 39 warnings inside dependencies: one deprecated `NetworkClient::flush()` call in core WebServer and 38 missing-field-initializer warnings from ESP-IDF HAL SPI/flash structures initialized with `{0}`. Those aggregates are zero-initialized in C++; the warnings do not indicate uninitialized spa state. They remain upstream issues; no vendor library was patched. Cached final builds may not repeat them. Firmware-source unsigned-clamp warnings encountered during repair were resolved. See final compile logs for the exact emitted diagnostics.

## Control inventory and ordering

Startup initializes outputs OFF, loads and validates configuration/runtime/fault latch, optionally starts authenticated Wi-Fi, initializes I2C/ADC, and acquires initial sensor readings. ADC startup failure latches a fault. Restored faults prevent pumps/UV from running. The loop processes bounded serial input and HTTP, acquires temperature every 500 ms, runs control every 250 ms, and prints diagnostics every second. Network handling and I2C still share the main loop; hardware timing has not been measured.

The LOW arbiter combines explicit LOW, heating demand, periodic temperature-sampling circulation, filtration quota/catch-up, postheat run-on and after-use. HIGH takes precedence over LOW, subject to the global fault gate. Pump changeover removes both outputs for at least 750 ms. All heater outputs are removed before switching away from LOW. UV requires healthy LOW circulation and an active sanitation/heat demand. Light is independent and is not forced off by a fault.

Heater model permission requires an authorised schedule/manual mode, no fault/lockout, healthy agreeing fresh sensors, high-limit clearance, requested and actual LOW, seven seconds circulation, qualification and contactor settling. The thermostat uses Ready/Standby hysteresis; raw overtemperature and thermal-rate faults override it. Main has no successful physical qualification path. Demo uses simulated qualification and a five-second stabilisation interval; physical heater pins stay OFF regardless.

Reachable operational states: BOOT/SELF_TEST during startup, then IDLE, CIRCULATING, HEATING (Demo logical model only), PUMP_HIGH, FAULT and LOCKOUT. FILTER, FREEZE_PROTECT and SERVICE enum values are reserved; filtration reports CIRCULATING. No freeze-protection or service-mode implementation exists. Several fault enums/constants and feedback pin placeholders reserve future work; they do not implement contactor/current/flow diagnostics.

## Relay assignment review

| Channel | GPIO | Requested function | Software result |
| --- | --- | --- | --- |
| CH1 | 1 | Heater safety contactor | Physical output inhibited in both builds |
| CH2 | 2 | Pump LOW | Shared pump interlock/deadtime arbiter |
| CH3 | 41 | Heater SSR control | Physical output inhibited in both builds |
| CH4 | 42 | Pump HIGH | Shared pump interlock/deadtime arbiter |
| CH5 | 45 | Ozone/UV | Healthy eligible LOW circulation only |
| CH6 | 46 | Spa light | Independent command |

GPIO/channel assignment matches the manufacturer's interface table: https://docs.waveshare.com/ESP32-S3-Relay-6CH . Board/module specifications: https://www.waveshare.com/product/modules/esp32-s3-relay-6ch.htm . This confirms documented mapping, not the wiring/polarity of David's physical unit.

## Executed scenario results

Each case runs in a fresh process; power-save/power-recover additionally exchange actual mock NVS data through a file. During simulated loop steps the test asserts mutual exclusion of LOW/HIGH pins and physical CH1/CH3 inhibition. The rapid-command case sends 10,000 deterministic conflicting commands. All time advances are virtual; the one-hour scenario does not wait one wall-clock hour.

| Build | Case | Expected outcome (full assertions in test source) | Result |
| --- | --- | --- | --- |
| Demo | `startup` | safe startup; safe heater startup | PASS |
| Demo | `heat` | logical heat qualified; demo never drives heater; setpoint stops heating | PASS |
| Demo | `hysteresis` | standby 3C band; ready 1C band | PASS |
| Demo | `changeover` | LOW on; both off deadtime; deadtime at 500ms; HIGH at 750ms | PASS |
| Demo | `fault-high` | sensor fault stops pump UV; fault rejects HIGH | PASS |
| Demo | `sensor-b` | B invalid | PASS |
| Demo | `overtemp` | overtemp wins heat; unsafe reset refused; safe reset accepted; persistent latch cleared | PASS |
| Demo | `disagreement` | persistent disagreement fault | PASS |
| Demo | `diff-inhibit` | 1.5C difference inhibits heat | PASS |
| Demo | `highlimit` | limit trips even idle | PASS |
| Demo | `invalid-range` | demo enforces sensor range | PASS |
| Demo | `filtration` | before solar window; 11AM filtration UV; 16:00 exclusive | PASS |
| Demo | `schedule-heat` | 10:59 no heat; demo clock used by heating; 16:00 no heat | PASS |
| Demo | `overnight` | overnight before midnight; overnight circulation; overnight after midnight; overnight end | PASS |
| Demo | `equal-window` | equal window closed | PASS |
| Demo | `catchup` | catchup quota; quota stops filtration | PASS |
| Demo | `afteruse` | jets trigger LOW UV; counts actual LOW time; after-use finishes | PASS |
| Demo | `afteruse-pause` | fault pauses after-use; after-use resumes after reset | PASS |
| Demo | `jets-timeout` | jets 20 minute timeout | PASS |
| Demo | `ready-timeout` | ready expires | PASS |
| Demo | `runtime` | runtime seconds accounting; runtime persisted | PASS |
| Demo | `day-rollover` | midnight reset | PASS |
| Demo | `nonfinite` | nonfinite filtered sensor fails closed | PASS |
| Demo | `filter-threshold` | 56 hour reminder; calendar clean age; 14 day reminder | PASS |
| Demo | `clock-backwards` | backward day correction does not reset quota | PASS |
| Demo | `stale` | stale measurements refused | PASS |
| Demo | `json` | JSON null for invalid sensors | PASS |
| Demo | `rapid-commands` | no false fault in command stress | PASS |
| Demo | `rollover` | pump timer across millis wrap | PASS |
| Demo | `wifi-loss` | control independent of WiFi status; WiFi reconnect attempted; fault wins during WiFi loss | PASS |
| Demo | `malformed` | strict setpoint; strict window; strict demo temp; strict clock | PASS |
| Demo | `power-save` | fault saved before simulated power cut | PASS |
| Demo | `power-recover` | fault restored in fresh process; manual heat off, sanitation restored after power cut; fault blocks recovered demands | PASS |
| Demo | `web-auth` | unauthorised request blocked; authenticated HIGH; fault rejects authenticated command | PASS |
| Demo | `web-input` | missing command; oversized command | PASS |
| Demo | `heat-runon` | heat initially on; heat off keeps runon; runon ends | PASS |
| Demo | `fault-v-schedule` | schedule initially heating; fault wins manual and schedule | PASS |
| Demo | `rapid-rise` | heat on before rate test; rapid rise lockout | PASS |
| Demo | `runon-wrap` | no phantom runon near wrap | PASS |
| Demo | `clock-loss` | unknown clock fails closed | PASS |
| Demo | `clean-confirm` | clean confirmation required; clean counter reset | PASS |
| Demo | `corrupt-config` | corrupt calibration lockout | PASS |
| Demo | `restart-fault` | fault survives restart | PASS |
| Demo | `restart-manual` | manual demand not resumed | PASS |
| Demo | `restart-afteruse` | after-use restored | PASS |
| Main | `main-injections` | Main rejects injection; Main rejects demo clock | PASS |
| Main | `main-lock` | Main lockout | PASS |
| Main | `adc-startup` | ADC startup lockout | PASS |
| Main | `adc-disconnect` | ADC disconnect fault | PASS |
| Main | `adc-timeout` | bounded conversion timeout | PASS |
| Main | `ntc` | NTC at 25C; divider; short; open | PASS |
| Main | `main-ema` | real EMA and slope | PASS |
| Main | `raw-overtemp` | raw overtemp bypasses EMA | PASS |
| Main | `corrupt-config` | corrupt calibration lockout | PASS |
| Main | `restart-fault` | fault survives restart | PASS |
| Main | `restart-manual` | manual demand not resumed | PASS |
| Main | `restart-afteruse` | after-use restored | PASS |

## Persistence and filtration findings

Setpoint, calibration values, heat mode and window are loaded from `spa` NVS. LOW/HIGH requests, Ready and logical heater states are volatile. Manual heat is made OFF on boot; solar can resume. Daily LOW seconds, since-clean LOW seconds, clean epoch/day and after-use remainder are checkpointed every 15 minutes. Jets entry/exit and explicit after-use requests checkpoint immediately. A first fault is checkpointed immediately after removing outputs. Fault resets require healthy fresh inputs and high-limit clearance, plus valid configuration/ADC initialization.

LOW seconds measure commanded relay state, not measured flow. At a day boundary, at most roughly one control interval can be attributed/discarded around reset. Counter checkpoints can lose up to roughly 15 minutes of credit on power failure; saved after-use work can replay a conservative remainder. When time is invalid, a trustworthy day cannot be assigned; first valid day resets the daily quota. Backward day corrections do not grant a new quota. The initial clean epoch is unknown until a clean acknowledgement occurs with valid time; runtime reminders still operate. No sanitation adequacy claim is made.

## Unresolved and untested items

| Item | Status / consequence |
| --- | --- |
| Physical safety, mains and hydraulic commissioning | NOT TESTED. Required checklist is `COMMISSIONING_CHECKLIST.md`. No electrical certification or live-heating approval. |
| Pump auxiliary feedback/electrical interlock | Not implemented in firmware; hardware must independently prevent simultaneous windings and handle welded contacts. A commanded OFF flag is not feedback. |
| High-limit input | `PIN_HIGH_LIMIT_OK=-1`; Main fails closed. Main fault acknowledgement cannot succeed until this interface is physically implemented. |
| Flow, dry-fire, independent thermal cutoff, SSR/current feedback | Not implemented/validated. Temperature agreement and rate diagnostics do not prove flow or SSR isolation. |
| ADC faults beyond ACK/timeout/plausibility | A responding ADC stuck at a plausible voltage or corrupted-but-plausible samples may escape software detection. Actual I2C fault latency and sensor curve remain unvalidated. |
| Wi-Fi/HTTP/NTP | Mocked interruption, reconnect attempt, authentication and remote-command gates executed. Real RF, NTP acquisition, DST, slow clients, HTTP resource exhaustion and scheduler latency NOT TESTED. HTTP Basic is plaintext; LAN isolation required. |
| Persistence media | Host mocks exercise logic and fresh-process restoration, not ESP32 NVS wear, failed writes, torn multi-key updates or brownout behavior. NVS API return values are not comprehensively handled. |
| Whole-loop deadline/watchdog | No dedicated application watchdog is configured. ADC polling and serial work are bounded, but network/I2C/flash timing can delay control. Bootloader/reset GPIO states require instruments. |
| Local panel/freeze protection | VL260 integration and freeze protection absent. Reserved enums are not working features. |
| Defaults / OFF semantics | Four-hour filtration, ozone duty and cleaning thresholds need equipment validation. `off`/`alloff` release manual demand; automatic LOW may resume. They are not emergency isolation. |
| Power recovery | Software tests executed with mock persistence. Real power cuts/reset reasons/welded outputs and motor restart policy remain hardware acceptance items. |
| Source compatibility | Builds verified only for the pinned toolchain/libraries and stated board settings. Other versions/configurations are not marked passed. |

Complete the physical checklist before any future live-heating revision. Retain this heater inhibition until those protections and their firmware interfaces have been separately implemented, reviewed and validated.

Final shared firmware SHA-256: `28c6c08c72816e2f9d485c438e91fc845aa6e47e019fb04cf8957da6e5ad9be6`.
