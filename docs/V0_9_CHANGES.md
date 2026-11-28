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

## Revision 2 — firmware-in-the-loop simulation round

V0.9 was then tested the same way the earlier design was. The **real** `SpaController.h` (Main build, onboard-ADC path) was compiled against a physical spa model (`firmware/sim/`). The model covers:
- an 800 L tub, a 2 kW heater, and the M7 sensors following the Balboa table
- 2-speed pump flow and the contactors with the NC interlock
- the LOW spare-pole heater locks, the 44 °C high-limit feeding both 24 V heater branches, and the thermal fuse
- the 2-wire light and the 24 V supply
- power cuts as real reboots, with flash memory kept

The safety rules are checked every 100 ms. The harness uses `SPA_SIM_COMMISSIONED`, which refuses to compile for the ESP32, and follows the firmware's logical heater commands. The physical CH1/CH3 lock in shipping code is unchanged.

**Result:** 103 / 103 scenarios pass, including 60 randomised fuzz runs (random commands, button mashing, sensor glitches, flow dips and 24 V sags over 6 h each). Across every single-fault scenario, the simulation never saw:
- LOW and HIGH together
- the heater without LOW, or with HIGH
- the heater during priming
- UV running without the pump (except during 24 V sags, see below)

### Firmware fixes from this round

| Finding (simulation) | Fix |
|---|---|
| Thermostat stopped ~0.35 °C below setpoint (37.68 for 38): while heating, the average includes the heater outlet | While the SSR heats, water temperature = the **cooler** probe (the inlet, whichever way round the sensors are wired) |
| Frozen readings, open element, failed SSR/contactor/spare pole were **silent** (frozen readings heated the tub to 43 °C until the high-limit) | **No heat response**: SSR on 2 min with LOW flowing and outlet/inlet difference < 0.25 °C → `HEATER_CONTACTOR_FAULT` lockout (caught at 390 s; frozen case held at 30.2 °C) |
| A 1 s sensor dropout stopped the spa until a manual reset | Transient sensor faults **auto-clear** after 60 s healthy; 3 in 24 h latch |
| ADC noise false-tripped the V0.8 "rise with SSR off" check → SSR lockout | The rise must persist 3 s (a stuck SSR gives a sustained rise; noise does not). V0.9 SSR proof window also uses persistence. |

A 2 s persistence on the no-flow rate check was tried and **rejected**: flow loss shows as a brief rate split between the probes, and persistence let it go undetected (35–53 s dry). The no-flow check is unchanged.

### Known limits (documented, not firmware bugs)

- **Onboard ADC noise:** clean at 3 mV residual noise after averaging. At 6 mV, false no-flow trips lock out. Bench acceptance: log both sensors at a steady temperature for 10 min; the reading spread must stay within ±0.1 °C. Otherwise improve filtering or fit the ADS1115.
- **24 V sag in the onboard build:** UV/ozone stays on for the length of the sag while the pump contactors drop out. In the ADS1115 build, the A3 supply monitor sees the sag.
- **Light:** if the real reset time is longer than `LIGHT_RESET_OFF_MS`, or the light remembers its colour, the firmware's idea of the colour drifts. Measure on the bench.
- **Manual heat mode is not resumed after a restart** (V0.6.2 policy). Solar mode is resumed.
- **Hot weather:** water that reaches ~44 °C from sun/ambient trips the hardware high-limit (manual reset). The firmware warns from 42 °C and does not latch on its own.
- **Main has no heater-qualification procedure yet** (V0.8 "NOT implemented"). The simulator grants it from the high-limit input, as the demo does. It must be implemented before commissioning.
- **Not detectable without feedback contacts:** a welded safety contactor, a welded LOW contactor (jets HIGH silently unavailable).

### Updated test totals

| Suite | Result |
|---|---|
| Core + V0.9 scenarios | 183 / 183 (141 original + 42 V0.9) |
| Firmware-in-the-loop simulation (`firmware/sim/run_sim.py`) | 103 / 103 |
| Live API / regressions (both wrappers) / topside / web app | 42 / 141 / 50 / 40 — all pass |

## V0.9.2 (Nov 2026)

- **Wi-Fi firmware update.** With `SPA_OTA_PASSWORD` defined (sketchbook file `SpaOtaCredentials.h`, never committed), Service Mode → Settings → Firmware update opens a 10-minute ArduinoOTA window (host `fisher-spa`). Requires Arduino ESP32 core 3.3.12+ with app rollback, and the 16M (3MB APP/9.9MB FATFS) partition scheme (two 3 MB app slots). A new image must come up in Service Mode with healthy probes, watchdog and Wi-Fi within 60 s, or it rolls back.
- **Heater restart hold.** After every restart (power loss, 45 °C cut-off trip, OTA), heating stays held until acknowledged in the dashboard (`POST /api/heater-restart-ack`). The 45 °C manual-reset cut-off interrupts all 24 V including the ESP32, so a trip cannot be distinguished from a power cut.
- **Live M7 heater path, still locked.** `M7Qualification.h` and `M7HeaterCycle.h` implement 3-minute circulation, a 30 s contactor-only SSR proof, a timed pulse and a 90 s heater-off observation. CH1/CH3 can only be driven when the sketch defines `SPA_M7_COMMISSIONING_LIMITS` and `SPA_M7_INDEPENDENT_CUTOFF_VERIFIED 1` (compile error otherwise); normal thermostat heating needs `SPA_M7_NORMAL_HEATING_VERIFIED 1` as well. The shipped sketches define none of these, so both heater outputs stay physically off.
- **First supervised heater profile** `SPA_M7_FIRST_PROFILE` (in `M7Qualification.h`) for the 1500 W element: 18 s pulse (Balboa M-7 manual: 6.5–18 s by rating), 90 s observation, A/B baseline ≤ 1.0 °C after ≥ 2 min, rise 0.15–1.5 °C, A/B ≤ 1.2 °C during the cycle, recovery ≤ 0.3 °C held 20 s, 40 °C maximum. Enable it only in the local sketch, with `SPA_M7_INDEPENDENT_CUTOFF_VERIFIED 1`; it runs one qualification and does not continue heating unless `SPA_M7_NORMAL_HEATING_VERIFIED 1` is added after a good logged pass. Rise limits are to be confirmed from the first logged pulse.
- Simulator: `run_sim.py` also builds the live M7 path with this profile (scenarios L01–L07: good flow passes with an 18 s pulse; no flow, weak flow, shorted SSR and open element fault; jets HIGH never heats; with normal heating enabled it heats 34→38 °C).
- `SPA_RAIL_REF_FITTED` can now be set from the sketch (for the ADS1115 rail reference on A2).
- Dashboard: numbered light programs, 10-minute A/B probe log download, restart-hold acknowledgement, firmware-update window.
- Heater element corrected to 1500 W.
- Tests: host stub gains `ESP_RST_UNKNOWN`; `verify_invariants.py` checks the new heater gate; the plant simulator acknowledges the restart hold after each boot and new scenario P05 proves heating stays off until acknowledged.

## Google Home (optional, Matter) — Nov 2026

Off unless the sketch defines `SPA_MATTER 1` (Main build, ESP32 core 3.1+ Matter library). Exposes three Matter endpoints only:

| Google Home | Matter type | Controller action |
|---|---|---|
| Spa temperature | Temperature Sensor (read-only) | water temperature, every 10 s while valid |
| Spa light | On/Off Light | `light on` / `light off` |
| Spa wake up | On/Off Plug | `ready` (1 °C band for 2 h, within the heat mode) / leave Ready |

No setpoint, jets, heater, Service Mode, fault reset, restart-hold or firmware control. Matter callbacks only queue the request; the loop task applies it through `executeCommand()` with the dashboard's Service/fault/settings lockout and mirrors the real state back (a refused request snaps back; the Ready timeout shows as off). Host tests: `live-tests/matter.cpp` with a stand-in for the Matter library (9 cases, part of `run_live.py`).

Setup: add `#define SPA_MATTER 1` to `SpaMain_Live.ino`, keep the 16M (3MB APP/9.9MB FATFS) scheme and check Verify reports the sketch fits the 3 MB app slot (it is not yet known whether the full firmware plus Matter fits). Needs a Google hub on the home Wi-Fi (Nest Mini/Audio/Hub, Google TV Streamer), 2.4 GHz Wi-Fi with IPv6, and a Google Home Developer Console test device with VID 0xFFF1 / PID 0x8000. The pairing code is printed on Serial and recorded once in the dashboard event history (`MATTER_PAIRING`).

## Topside update (Nov 2026)

- Screen blackout: display panel and backlight turn off after `SPA_TOPSIDE_SCREEN_TIMEOUT_MS` (default 1 hour) without a button press. The waking press only wakes the screen. Fault and Service wake it and keep it lit.
- `SPA_TOPSIDE_ROTATION` selects the display rotation (set to 3 for the fitted panel).
- `SPA_TOPSIDE_BACKLIGHT_VERIFIED` set to 1 after inspecting the fitted module (onboard Q2 enable transistor).
- Hardware decisions recorded in `TOPSIDE_V08_HARDWARE.md` (12-pin topside cable), `M7_SENSOR_INTERFACE.md` (sensor front end) and `HARDWARE_INTERFACE.md` §4.4–4.5 (tube-mounted high-limit and thermal fuse).
- Enclosure print files in `stl/`.
- Blackout logic host-tested only; not compiled for ESP32 or run on hardware.

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

## V0.9.3 — live commissioning, 26–28 Nov 2026

Changes made from the first supervised heater runs, the first Wi-Fi updates and the first Google Home pairing.

| Area | Change | Why |
|---|---|---|
| Heater test (M7 qualification) | Pulse 18 s → **45 s**; baseline = mean of the last 10 s of circulation; recovery creep 0.10 → 0.15 °C | Spa is ~400 L with a 1.5 kW element and high LOW-speed flow. With an averaged baseline an 18 s pulse rose only 0.09–0.19 °C. The 45 s pulse gave 0.27 °C live. |
| No-heat-response check (fault 14) | Was: \|A−B\| ≥ 0.25 °C after 2 min. Now: 15 s-smoothed signed A−B must shift ≥ 0.08 °C from the pre-heat reference within 3 min (skipped if re-energised within 3 min). Backup: the cooler probe must rise ≥ 0.15 °C per 45 min of heater-on time. | A working heater only moves A−B by 0.10–0.20 °C here; the old check faulted on every normal heating run. Replay of the live run: shift seen 30 s after SSR on. 1 h of live heating with no fault (20.8 → 23.9 °C). |
| Wi-Fi update | Hold released when the 10 min window ends (and cleared by the loop if a window overruns); 409 now names the failing check and logs `OTA_REFUSED`; ArduinoOTA keeps off mDNS in Matter builds | Window expiry left uploads and Service exit blocked; a closing window crashed the Matter build. Upload with `espota.exe -i <spa IP> -p 3232 -P 52000 -a <password> -f <bin>` and a Windows firewall rule for TCP 52000. |
| Google Home (Matter) | Station IPv6 enabled; BLE commissioning off (pair on the network); names set; fresh probe read after `Matter.begin()` | "Can't reach device" on first pairing; Espressif warns BLE commissioning conflicts with sketch-held Wi-Fi credentials. |
| Dashboard | Plain names for faults 14 (Heater not heating) and 15 (Unexpected heat with heater off) | Showed as generic controller fault. |
| Late probe readings | A reading older than 2 s turns the heater off at once (pump keeps running) but is no longer a fault; only a loss over **10 s** latches SENSOR_STALE. After the heater test has passed, a late/invalid reading pauses the SSR and heating resumes when readings return; during the test it restarts the test (2 retries per heating call, then fault). History now records which check tripped (M7 phase + reason, inhibit cause, or rate values) and RAPID_TEMP_RISE has its own label. | A 3 s loop stall latched the spa off (sim L10/L11 fail on the old code). Owner: one missed report is not worth a shutdown. The heater is still never on with a reading older than 2 s; the 5 s task watchdog covers longer stalls. |
| Topside | Recovery controls: **Up+Down 3 s** acknowledge restart; **Light+Down 5 s, then Light** reset fault; **Jets+Light 5 s, then Jets** exit Service Mode; **Jets 5 s** emergency stop. Countdown/confirm prompts; any other key cancels; confirmation expires after 10 s. | Recovery was web-only. Same controller checks as the dashboard. |

Known follow-ups: Settings/Diagnostics still show static "Matter not integrated" and do not reflect pairing; set a fixed DHCP address for the controller.
