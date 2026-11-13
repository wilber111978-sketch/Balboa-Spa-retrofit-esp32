# Install Commissioning Self-Test

Runs **once** when the controller is first installed on the spa, then never again unless re-run from Service mode (e.g. after replacing a sensor, the pump or the light).

The controller cannot see the light or the pump, so the test has two parts:

- **Part A — automatic:** everything the controller can measure itself. Runs unattended.
- **Part B — guided:** things only a person can see. The topside asks Yes/No questions; you answer with the buttons.

Results are stored in NVS (wear rules: [PROVISIONING_AND_UPDATES.md](PROVISIONING_AND_UPDATES.md)), shown in the app (Diagnostics → Install test), and written to the event log.

---

## When it runs

- An NVS flag `commissioned` is false on a new controller.
- **Part A** starts automatically after the first priming completes. Its heater test runs on the first normal heat call, so nothing extra is needed.
- **Part B** is offered on the topside after Part A passes: "Run install test? Temp+ = Yes, Temp− = Later". "Later" asks again at next power-up, or the user can start it from the app.
- The flag is set once both parts are complete. Each part's results are saved separately, so a part does not need repeating if the other is interrupted.
- Service mode → "Re-run install test" clears the flag and runs it again.

Until Part A has passed, the controller runs normally but with the **conservative default thresholds**, and the app shows "Install test pending".

---

## Part A — automatic checks

| # | Check | Pass condition | On fail |
|---|---|---|---|
| A1 | I²C devices | ADS1115 (0x48) and DS3231 (0x68) respond | fault, test stops |
| A2 | 3.3 V reference | ADC A2 within 3.2–3.4 V | SENSOR_RAIL_RANGE |
| A3 | Sensors in range | both sensors valid, pump stopped | SENSOR_*_RANGE |
| A4 | Sensor agreement on LOW | after LOW settle, A and B agree within tolerance (heater off) | SENSOR_MISMATCH |
| A5 | Sensor agreement on HIGH | after HIGH settle, A and B agree within tolerance | SENSOR_MISMATCH |
| A6 | RTC | DS3231 time valid (or set from NTP) | TIME_INVALID warning only |
| A7 | **Heater response + sensor orientation** | on the first heat call: within the response timeout, one sensor rises clearly before the other | HEAT_NO_RESPONSE |
| A8 | **Baseline capture** | records steady-state heating delta-T, rise rate and time-to-respond on LOW | — |

### A7 — automatic inlet/outlet detection

When the heater first switches on with pump LOW running, the **outlet** sensor rises within seconds while the inlet stays close to tub temperature.

- The controller records which sensor rose first and stores it as the outlet.
- If that is the opposite of the wiring assumption (A = inlet, B = outlet), it swaps them in software. No rewiring is needed.
- The app shows the result: "Sensor B confirmed as heater outlet".

A pass also proves the whole heater chain works end-to-end: LOW contactor spare poles, high-limit, safety contactor, SSR and element.

### A8 — baseline and suggested thresholds

The first full heating run on LOW records:

- normal heating delta-T (expected well under 1–2 °C for a 2 kW heater with good flow)
- outlet rise rate after heater on
- time from heater on to measurable delta-T

The app then shows **suggested** trip thresholds (baseline × safety margin) for DELTA_T_HIGH, NO_FLOW_SUSPECTED and HEAT_NO_RESPONSE. They are applied only when the user accepts them; until then the conservative defaults stay in force.

### Not tested automatically

Deliberately commanding the heater with no flow, to prove the LOW-contactor permissive blocks it, is **not** done. That would risk dry-firing the element. The permissive is proved on the bench with no mains load instead ([HARDWARE_INTERFACE.md](HARDWARE_INTERFACE.md) §10, step 7).

---

## Part B — guided checks

Answer on the topside: **Temp+ = Yes**, **Temp− = No**, **Jets = repeat that step**, **Light = skip**. The app shows the same prompts and accepts answers too.

The tests run with the spa full. Best done at dusk so the light is easy to see.

| # | Step | Question |
|---|---|---|
| B1 | Pump LOW on | "Is water circulating gently?" |
| B2 | Pump LOW → HIGH (after the normal sequence) | "Are the jets on strong?" |
| B3 | HIGH off | "Have the jets stopped?" |
| B4 | UV / ozone with pump on | "Is the ozone / UV indicator on?" (skip if not visible) |
| B5 | Light on | "Is the light on?" |
| B6 | **Light colour count** | single step pulses, showing "Step 1, 2, 3…" → "Has it come back to the first colour?" Yes on step N sets `LIGHT_PROGRAM_COUNT` = N |
| B7 | **Light step timing** | tries off-times of 200 ms, 500 ms, 1 s, 1.5 s → "Did the colour change?" The shortest reliable time, plus a 200 ms margin, becomes `LIGHT_STEP_OFF_MS` |
| B8 | **Light reset time** | steps to colour 2, then turns off for 5 s, 10 s, 15 s → "Is it back on the first colour?" The first Yes, plus a 5 s margin, becomes `LIGHT_RESET_OFF_S` |
| B9 | Buttons | "Press each button once": each press is shown on screen, confirming wiring and debounce |

If the user skips B6–B8, the defaults stay (7 programs, 500 ms, 10 s) and can be adjusted later in Service settings.

A phone video of the spa during B6–B8 makes counting easier. It can be done by one person.

---

## Results and report

Stored per check: pass / fail / skipped, measured values, timestamp, firmware version.

App: Diagnostics → Install test shows:
- each check with its result and value
- sensor orientation result
- baseline heating figures and suggested thresholds (Accept / Keep defaults)
- light settings found
- "Re-run install test" (Service mode only)

Topside, at the end: "Install test complete — 14 passed, 1 skipped".

Any failure is logged with the normal fault record (probable causes and recommended checks, [FAULTS.md](FAULTS.md)), so a failed install test reads the same as a live fault.

---

## Safety rules

- Every output is driven through the normal safety state machine. The test only *requests* states, exactly like the buttons or app.
- The heater is never commanded without pump LOW confirmed running, and never during Part B.
- The 2 s input lockout, the dead-times and the heater-off sequence all apply.
- Any real fault during the test aborts it, and the normal fault handling takes over.
