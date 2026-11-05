# Spa Controller V0.6.2 — safeguards implementation and validation

Date: 5 November 2026, Sydney. Based on the reviewed V0.6.1 source. No firmware was flashed and no hardware was connected.

## Result

**141 executed host scenarios passed: all 57 original regression identities plus 84 new cases (42 exercised in each build). Both Main and Demo compile.** Main commissioning remains locked and both builds physically inhibit CH1/CH3. No relay assignment or established scheduling rule was changed. These results support isolated bench validation, not electrical certification or live-heater approval.

## Implemented behavior

| Area | Implementation | Validation |
| --- | --- | --- |
| Persistent Service Mode | `alloff` / `service on` immediately stop all six outputs and verify a saved inhibit; scheduling, manual starts and light cannot override it. Explicit `service off confirm` is required; faults/settings failures block exit. After-use pauses. | Startup restoration in a new process; active loads; after-use; schedule and remote-command attempts; failed exit save. |
| Shutdown commands | `off` stops manual jets/pump demand while retaining established automatic LOW/after-use behavior. Its response explains this. `heatmode off` retains its existing meaning. `alloff` is no longer an alias for `off`. | Existing after-use cases retained; separate OFF/alloff tests. |
| Watchdog | ESP-IDF TWDT user with 5-second timeout and panic/reset enabled, fed at completed-loop boundary. Reconfigures an already initialized watchdog. Init/feed failures fail closed. TASK/INT/WDT/panic and brownout restart reasons enter persistent Service Mode and latch the reason. | API/config/feed mocks, error injection and fresh-process restart paths. Actual hardware timeout/reset NOT tested. |
| Settings integrity | Configuration, safety state and runtime each use a version-2 NVS blob, CRC32, semantic validation, checked writes and readback. Presence bits prohibit silent fallback after migration. Legacy values preserved on first migration. Failure stops outputs, enters Service Mode and does not acknowledge success. | Open/write/short-write/readback failures, corruption, valid-CRC unsupported version, truncated/missing record, semantic invalidity, round trip, legacy migration and CRC32 standard vector. |
| Sensor freshness | Each channel's old timestamp is checked before command/network work and before acquisition can refresh it. Over 2 seconds old latches `SENSOR_STALE` and removes heater/pump/UV outputs. Unsigned elapsed-time arithmetic supports millis wrap. | Delayed-loop pre-refresh fault, one stale channel, wrap and safe fresh acknowledgement in both builds; original sensor scenarios retained. |
| Network work | Bounded HTTP parser replaces synchronous WebServer. One client; 128 RX and 128 TX bytes per poll; header/body/response limits; nonblocking socket IO; absolute 1.5-second receive and transmit deadlines; one command per completed request. Authenticated routes retained. | Actual parser with mocked sockets: authentication, size limits, partial body/header, slow/non-reading client, ambiguous content lengths, byte budgets, status JSON and Service Mode rejection. |
| Fault recovery with heater uncommissioned | Main can acknowledge recovered sensor/pump faults without an unassigned high-limit; assigned high-limit inputs must still be healthy. Main heater permission immediately rejects uncommissioned operation, and the physical gate is unchanged. | Main lock regression, stale acknowledgement and fault/Service exit tests. |

## Compilation

| Build | Compiler result | Flash / maximum | RAM / maximum | Emitted warnings |
| --- | --- | --- | --- | --- |
| Main | PASS, exit 0 | 955231 / 3145728 bytes | 46816 / 327680 bytes | 0 |
| Demo | PASS, exit 0 | 954755 / 3145728 bytes | 46840 / 327680 bytes | 0 |

Same pinned configuration as V0.6.1: Arduino CLI 1.3.1, ESP32 Arduino core 3.3.2, Adafruit ADS1X15 2.5.0, Adafruit BusIO 1.17.4. Core libraries come from 3.3.2. FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=disabled,CDCOnBoot=cdc`. Default 240 MHz, QIO/80 MHz flash, hardware USB CDC/JTAG; USB CDC enabled. The actual fitted board/module still needs checking.

Compiler warnings were enabled. Installed dependencies and existing build paths were reused. Final emitted diagnostics are in the compile logs. Cached dependencies were not forcibly rebuilt merely to repeat previously reviewed vendor warnings. Removing the synchronous WebServer dependency also removes its deprecated flush path from this firmware. No firmware-source warnings were emitted in these builds. No APK or upload was produced.

## Regression preservation

`tests/verify_invariants.py` executed successfully. It compares the release with the frozen V0.6.1 source and verifies identical relay GPIO constants, identical `schedulePermitsHeating` implementation, unchanged scheduling constants, the same 57 baseline case identities with passing results, and both heater lock mechanisms.

The LOW arbitration schedule is unchanged apart from the new explicit Service/storage inhibit. Default 11:00–16:00 window, overnight/equal-window handling, preferred/catch-up filtration, four-hour quota, temperature-sampling cadence, Ready/Standby thresholds, jets timeout and after-use duration remain intact.

Test fixtures changed where required to exercise the new implementation: remote-command cases now send real HTTP bytes through the production parser; persistence cases inspect checked records and exchange complete mock blobs across processes; rollover fixtures establish realistic pre-wrap sensor timestamps rather than teleporting a live sensor 49 days into the future. The original 57 assertion objectives and case identities remain; their frozen source and results are retained for comparison.

## Executed results

`python3 tests/run_all.py` was executed successfully on the final source, running both suites and the invariant checks. All 141 scenarios passed. Host compilations use C++17 with `-Wall -Wextra -Werror`. The code under test is the same shared firmware/persistence/HTTP parser compiled for the ESP32, with peripheral interfaces mocked. A test is marked PASS only after its subprocess returned zero.

| Group | Build | Case | Executed result |
| --- | --- | --- | --- |
| Original regression | Demo | `startup` | PASS |
| Original regression | Demo | `heat` | PASS |
| Original regression | Demo | `hysteresis` | PASS |
| Original regression | Demo | `changeover` | PASS |
| Original regression | Demo | `fault-high` | PASS |
| Original regression | Demo | `sensor-b` | PASS |
| Original regression | Demo | `overtemp` | PASS |
| Original regression | Demo | `disagreement` | PASS |
| Original regression | Demo | `diff-inhibit` | PASS |
| Original regression | Demo | `highlimit` | PASS |
| Original regression | Demo | `invalid-range` | PASS |
| Original regression | Demo | `filtration` | PASS |
| Original regression | Demo | `schedule-heat` | PASS |
| Original regression | Demo | `overnight` | PASS |
| Original regression | Demo | `equal-window` | PASS |
| Original regression | Demo | `catchup` | PASS |
| Original regression | Demo | `afteruse` | PASS |
| Original regression | Demo | `afteruse-pause` | PASS |
| Original regression | Demo | `jets-timeout` | PASS |
| Original regression | Demo | `ready-timeout` | PASS |
| Original regression | Demo | `runtime` | PASS |
| Original regression | Demo | `day-rollover` | PASS |
| Original regression | Demo | `nonfinite` | PASS |
| Original regression | Demo | `filter-threshold` | PASS |
| Original regression | Demo | `clock-backwards` | PASS |
| Original regression | Demo | `stale` | PASS |
| Original regression | Demo | `json` | PASS |
| Original regression | Demo | `rapid-commands` | PASS |
| Original regression | Demo | `rollover` | PASS |
| Original regression | Demo | `wifi-loss` | PASS |
| Original regression | Demo | `malformed` | PASS |
| Original regression | Demo | `power-save` | PASS |
| Original regression | Demo | `power-recover` | PASS |
| Original regression | Demo | `web-auth` | PASS |
| Original regression | Demo | `web-input` | PASS |
| Original regression | Demo | `heat-runon` | PASS |
| Original regression | Demo | `fault-v-schedule` | PASS |
| Original regression | Demo | `rapid-rise` | PASS |
| Original regression | Demo | `runon-wrap` | PASS |
| Original regression | Demo | `clock-loss` | PASS |
| Original regression | Demo | `clean-confirm` | PASS |
| Original regression | Demo | `corrupt-config` | PASS |
| Original regression | Demo | `restart-fault` | PASS |
| Original regression | Demo | `restart-manual` | PASS |
| Original regression | Demo | `restart-afteruse` | PASS |
| Original regression | Main | `main-injections` | PASS |
| Original regression | Main | `main-lock` | PASS |
| Original regression | Main | `adc-startup` | PASS |
| Original regression | Main | `adc-disconnect` | PASS |
| Original regression | Main | `adc-timeout` | PASS |
| Original regression | Main | `ntc` | PASS |
| Original regression | Main | `main-ema` | PASS |
| Original regression | Main | `raw-overtemp` | PASS |
| Original regression | Main | `corrupt-config` | PASS |
| Original regression | Main | `restart-fault` | PASS |
| Original regression | Main | `restart-manual` | PASS |
| Original regression | Main | `restart-afteruse` | PASS |
| New safeguards | Demo | `service` | PASS |
| New safeguards | Demo | `alloff` | PASS |
| New safeguards | Demo | `off-semantics` | PASS |
| New safeguards | Demo | `service-fault-exit` | PASS |
| New safeguards | Demo | `service-save` | PASS |
| New safeguards | Demo | `service-reboot` | PASS |
| New safeguards | Demo | `watchdog-reboot` | PASS |
| New safeguards | Demo | `watchdog-panic` | PASS |
| New safeguards | Demo | `brownout` | PASS |
| New safeguards | Demo | `watchdog-feed` | PASS |
| New safeguards | Demo | `watchdog-feed-failure` | PASS |
| New safeguards | Demo | `watchdog-init-failure` | PASS |
| New safeguards | Demo | `watchdog-existing` | PASS |
| New safeguards | Demo | `settings-open-failure` | PASS |
| New safeguards | Demo | `settings-write-failure` | PASS |
| New safeguards | Demo | `settings-short-write` | PASS |
| New safeguards | Demo | `settings-readback` | PASS |
| New safeguards | Demo | `settings-crc` | PASS |
| New safeguards | Demo | `settings-version` | PASS |
| New safeguards | Demo | `settings-missing` | PASS |
| New safeguards | Demo | `settings-truncated` | PASS |
| New safeguards | Demo | `settings-semantic` | PASS |
| New safeguards | Demo | `settings-migration` | PASS |
| New safeguards | Demo | `settings-roundtrip` | PASS |
| New safeguards | Demo | `safety-crc` | PASS |
| New safeguards | Demo | `runtime-crc` | PASS |
| New safeguards | Demo | `fault-reset-save-failure` | PASS |
| New safeguards | Demo | `service-exit-save-failure` | PASS |
| New safeguards | Demo | `stale-before-refresh` | PASS |
| New safeguards | Demo | `stale-b` | PASS |
| New safeguards | Demo | `stale-wrap` | PASS |
| New safeguards | Demo | `network-slow-header` | PASS |
| New safeguards | Demo | `network-slow-body` | PASS |
| New safeguards | Demo | `network-budget` | PASS |
| New safeguards | Demo | `network-large-header` | PASS |
| New safeguards | Demo | `network-stalled-response` | PASS |
| New safeguards | Demo | `network-auth-status` | PASS |
| New safeguards | Demo | `network-duplicate-length` | PASS |
| New safeguards | Demo | `network-service` | PASS |
| New safeguards | Demo | `network-tx-budget` | PASS |
| New safeguards | Demo | `crc-known-vector` | PASS |
| New safeguards | Demo | `settings-legacy-values` | PASS |
| New safeguards | Main | `service` | PASS |
| New safeguards | Main | `alloff` | PASS |
| New safeguards | Main | `off-semantics` | PASS |
| New safeguards | Main | `service-fault-exit` | PASS |
| New safeguards | Main | `service-save` | PASS |
| New safeguards | Main | `service-reboot` | PASS |
| New safeguards | Main | `watchdog-reboot` | PASS |
| New safeguards | Main | `watchdog-panic` | PASS |
| New safeguards | Main | `brownout` | PASS |
| New safeguards | Main | `watchdog-feed` | PASS |
| New safeguards | Main | `watchdog-feed-failure` | PASS |
| New safeguards | Main | `watchdog-init-failure` | PASS |
| New safeguards | Main | `watchdog-existing` | PASS |
| New safeguards | Main | `settings-open-failure` | PASS |
| New safeguards | Main | `settings-write-failure` | PASS |
| New safeguards | Main | `settings-short-write` | PASS |
| New safeguards | Main | `settings-readback` | PASS |
| New safeguards | Main | `settings-crc` | PASS |
| New safeguards | Main | `settings-version` | PASS |
| New safeguards | Main | `settings-missing` | PASS |
| New safeguards | Main | `settings-truncated` | PASS |
| New safeguards | Main | `settings-semantic` | PASS |
| New safeguards | Main | `settings-migration` | PASS |
| New safeguards | Main | `settings-roundtrip` | PASS |
| New safeguards | Main | `safety-crc` | PASS |
| New safeguards | Main | `runtime-crc` | PASS |
| New safeguards | Main | `fault-reset-save-failure` | PASS |
| New safeguards | Main | `service-exit-save-failure` | PASS |
| New safeguards | Main | `stale-before-refresh` | PASS |
| New safeguards | Main | `stale-b` | PASS |
| New safeguards | Main | `stale-wrap` | PASS |
| New safeguards | Main | `network-slow-header` | PASS |
| New safeguards | Main | `network-slow-body` | PASS |
| New safeguards | Main | `network-budget` | PASS |
| New safeguards | Main | `network-large-header` | PASS |
| New safeguards | Main | `network-stalled-response` | PASS |
| New safeguards | Main | `network-auth-status` | PASS |
| New safeguards | Main | `network-duplicate-length` | PASS |
| New safeguards | Main | `network-service` | PASS |
| New safeguards | Main | `network-tx-budget` | PASS |
| New safeguards | Main | `crc-known-vector` | PASS |
| New safeguards | Main | `settings-legacy-values` | PASS |


Every regression loop step asserts LOW/HIGH physical mutual exclusion and physical CH1/CH3 inhibition. The existing stress case executes 10,000 conflicting commands. Service tests explicitly check all six pins. Time advances are virtual; no physical relay timing was measured.

## Remaining limits and commissioning work

- Electrical safety, relay polarity, boot-ROM/brownout GPIO behavior, contactor release, motor interlocking, flow, independent thermal protection, SSR/current feedback and hydraulic operation remain hardware acceptance work. Commanded OFF cannot prove a contact opened.
- The watchdog has been cross-compiled and its API/recovery paths simulated. Actual 5-second expiry, panic reboot configuration, reset reasons and relay drop-out must be measured on the isolated board. A stalled CPU cannot itself issue an immediate OFF command; independent protection remains necessary.
- Socket parsing is bounded in bytes and deadlines, with nonblocking recv/send. Real Wi-Fi/stack/accept/close/reconnect, flash commits and scheduler latency are not a proven hard real-time bound. Measure under weak RF, disconnects and slow/malicious clients. HTTP Basic remains plaintext and requires a trusted isolated LAN.
- Tests model NVS behavior, including failures, but do not prove flash wear or power-cut atomicity on the board. Canonical blob saves are checked; runtime/config/safety are deliberately separate records. CRC detects accidental corruption, not an attacker. A storage device that cannot save cannot guarantee persistence of a newly requested stop; commands report the failure and outputs stop in RAM.
- Integrity failures do not silently erase settings or auto-clear faults. Correct the stored record/NVS deliberately, restart, then acknowledge only with safe inputs. There is no remote bypass of the commissioning lock.
- Runtime checkpoint policy remains 15 minutes, with the existing immediate checkpoints. Power loss can lose credit or replay a conservative sanitation remainder. Service Mode prevents it running until explicitly exited.
- Sensor freshness detects missed updates, not a responding ADC stuck at a plausible reading. Physical curve calibration, failed ADC transfers and stuck sensors require board tests.
- VL260 integration, freeze protection and contactor/current feedback remain absent. Sanitation/ozone defaults require equipment validation.

Follow `COMMISSIONING_CHECKLIST.md`. Both heater pins remain inhibited throughout this release.

Final shared control source SHA-256: `166e140085765de3613b7107bbed3ad6f280c067aecde9cdcd7b28adffa93780`.
