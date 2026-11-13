# V0.9 — changes from V0.8

V0.9 is the first release that edits the reviewed V0.6.2 controller (`SpaController.h`). V0.7 and V0.8 deliberately wrapped it unchanged. Every change below is additive or narrowly scoped, and the physical heater outputs remain locked (`HEATER_OUTPUTS_COMMISSIONED = false`) exactly as before.

Sources for the changes: the design review, the 60-scenario plant simulation, and the owner's decisions on 13 Nov 2026.

## Already in V0.8, so not redone

| Item | V0.8 implementation |
|---|---|
| Both sensors treated symmetrically | water = average of A/B; difference is `fabs(A−B)`; rise-rate check on **either** sensor plus rate difference. A swapped sensor pair is therefore already handled. |
| Over-temperature | 42 °C trip (either sensor, raw or filtered), latched |
| SSR-stuck detection | contactor on, SSR off, fast rise → `HEATER_SSR_SUSPECT` |
| Thermostat hysteresis | ready 1 °C / standby 3 °C, so no rapid switching |
| Temperature sampling | 30 min interval, 2 min run (heat-available modes only; V0.9 extends this) |
| Jets timeout | HIGH auto-off at 20 min, then 60 min after-use circulation |
| Filtration | daily 4 h LOW target, preferred 11–13 / 15–16, catch-up |
| Remote safety | high-level commands only, HTTP Basic auth, 250 ms rate limit, confirm on service / fault reset |
| Topside | GC9A01 + 4 buttons, debounce, stuck-button detection, shared safe command path |

## Added or changed in V0.9

| # | Behaviour | Detail | Key constants |
|---|---|---|---|
| 1 | **Onboard ADC option (default)** | No ADS1115 needed yet. Sensors on GPIO8/9 (ADC1), 32-sample averaged calibrated millivolts; open circuit ≥ 3.00 V. Build with `SPA_TEMP_ADC_ADS1115=1` once the module is fitted. | `SPA_TEMP_ADC_ADS1115`, `ONBOARD_ADC_SAMPLES` |
| 2 | Optional ratiometric 3.3 V reference | Rail measured on GPIO3 (onboard) or A2 (ADS1115); plausibility 3.15–3.45 V, otherwise 3.300 V | `RAIL_REF_FITTED=false` |
| 3 | Optional 24 V monitor (ADS1115 A3) | below 20 V → heater inhibited; re-armed after 5 s above 21 V | `SUPPLY_MON_FITTED=false` |
| 4 | **UV/ozone on either pump speed** | on 1 s after LOW **or** HIGH starts; off when stopped, during changeover, in faults | `UV_SETTLE_MS=1000` |
| 5 | **Manual LOW timeout** | 20 min, then normal schedule control resumes | `MANUAL_LOW_TIMEOUT_MS` |
| 6 | **Economy floor** | solar mode, outside the window: heats only below 34 °C, back to 35 °C (or the setpoint if lower). Also applies when time is unknown. | `ECONOMY_FLOOR_C=35` |
| 7 | **Water temperature = circulating reading** | a reading counts only after 60 s of circulation at either speed; UIs show the last such reading, its age once ≥ 5 min, and `--.-` before the first one | `WATER_SETTLE_MS` |
| 8 | **Polls in every mode** | 30 min after the last circulating reading (heat OFF included); ends as soon as a settled reading has been averaged | `SAMPLE_INTERVAL_MS` |
| 9 | **Priming** | after a genuine power-on: 4 min pump LOW, heater locked; jets usable; Temp ± ends it early; skipped after watchdog / OTA / software restarts | `PRIMING_MS` |
| 10 | **Freeze protection** | either sensor < 5 °C → LOW until every valid sensor ≥ 8 °C. Runs through non-flow faults. Both sensors invalid for 10 min → 15 min LOW per hour. | `FREEZE_*` |
| 11 | **2 s input lockout** | jets and light, separately; rejected presses are not queued; OFF and light-off are never blocked | `INPUT_LOCKOUT_MS=2000` |
| 12 | **Light** | 40 min auto-off; colour programs by power-interruption steps; switch-on from a known state (waits out the reset time); restores the last colour; `lightprog N`, API `program`, topside long-press Light (when on) = next colour | `LIGHT_*` |
| 13 | **No-flow retry** | first and second trip in 24 h: pump off, retry after 15 min with a 30 s HIGH purge (not counted as jets / after-use); third trip latches | `FLOW_*` |
| 14 | **Hot water from ambient/sun** | if the SSR has been off ≥ 10 min (or never on since boot): `waterHot` warning, heater inhibited, clears below 41 °C. Heater-caused over-temp still latches at 42 °C. | `AMBIENT_HOT_SSR_OFF_MS`, `WATER_HOT_CLEAR_C` |
| 15 | **Sensor agreement at rest** | after 2 min circulation with the SSR off ≥ 2 min: difference ≥ 0.5 °C for 60 s → heating inhibited; clears below 0.4 °C | `CAL_*` |
| 16 | **Sensor cap** | no new heat if either sensor ≥ 40 °C; heater off if either ≥ 41 °C | `HEATER_SENSOR_CAP_C` |
| 17 | **SSR proof window** | contactor closes 30 s before the SSR; an A/B difference rise ≥ 0.3 °C in that window → `HEATER_SSR_SUSPECT` (covers the with-flow case the V0.8 rate check could miss) | `SSR_PROOF_MS` |

Interfaces:
- **API `/api/state`:**
  - `temperature.water_c` is now the circulating reading.
  - Added `water_valid`, `water_age_s` and `probe_average_c`.
  - Added `light.program`, `program_count` and `auto_off_s`.
  - Added a `v09` block: priming, freeze, water_hot, sensor_mismatch, economy_floor, no_flow_retry and temp_adc.
- **API `/api/light`:** accepts `{"on":true,"program":N}`. Input lockout returns 409.
- **Topside:** shows the reading age, a priming countdown and "Checking temperature…"; Temp ± during priming ends it; long-press Light while on steps the colour.
- **Web app:** shows the reading age, the priming countdown and a hot-water note.

## Hardware decisions recorded (docs only)

- LOW contactor spare poles 1 and 2 in the CH1 coil feed and the CH3 SSR supply: heater only while LOW is in, never with HIGH.
- Manual-reset high-limit, 44 °C, with a status input proposed on GPIO16.
- Thermal fuse ~70–75 °C on the heater body.
- Light on 12 V AC through CH6.

See `HARDWARE_INTERFACE.md` and `MASTER_GPIO_MAP.md`.

## Tests

| Suite | Result |
|---|---|
| Original core + extended scenarios (`tests/run_all.py`) | 141 / 141 |
| New V0.9 scenarios (`tests/run_v09.py`, Demo + Main onboard-ADC) | 39 / 39 |
| Live API (`live-tests/run_live.py`, both builds, + 3 V0.9 cases) | 42 / 42 |
| Regression under live wrapper and topside wrapper | 141 / 141 each |
| Topside (`topside-tests/run.py`, both builds, + 4 V0.9 cases) | 50 / 50 |
| Web app unit tests (`npm test`) | 40 / 40 |

The runners now build the Main ADS1115 mock path with `-DSPA_TEMP_ADC_ADS1115=1`.

These existing tests were changed on purpose, because V0.9 changes the behaviour they check:

| Test | Old expectation | V0.9 expectation |
|---|---|---|
| `rapid-rise` | first trip latches | first trip retries (retry pending, not latched) |
| `raw-overtemp` (Main) | 43 °C with heater never on latches | ambient → `waterHot` warning; with heater on → still latches |
| `network-slow-body` (Main) | pump request OFF | priming ended first; still checks no command executes |
| topside `jets-heating` | LOW then HIGH instantly | 2 s between presses (lockout) |
| topside setup | — | resets `primingActive` like other controller state |

`verify_invariants.py` was rewritten for V0.9. It still pins the relay GPIOs, the scheduling constants and the heater locks. It also requires all 57 reviewed baseline scenarios to be present and passing, and requires the V0.9 scenarios to pass. `FROZEN_V09.json` / `verify_frozen.py` now pin the V0.9 sources; `FROZEN_V062.json` is kept for history.

## Not verified here

- **ESP32 compile.** The toolchain download servers were not reachable from the build environment. Run `firmware/build.sh` and `firmware/build-live.sh` locally before flashing.
- **Hardware.** Relay polarity and boot behaviour, onboard-ADC accuracy against a thermometer, light step and reset timing, high-limit and thermal-fuse behaviour, and contactor heater locks. Follow the commissioning order in `HARDWARE_INTERFACE.md` §10.

## Planned for later versions

- Wi-Fi provisioning, setup screen and web token (`PROVISIONING_AND_UPDATES.md`)
- OTA updates via the app
- DS3231 RTC
- Matter
- Install self-test with light colour decode (`COMMISSIONING_SELF_TEST.md`)
- Local-only reset of latched faults via a topside button combination
- Light program names and persistence of the last colour across power loss
