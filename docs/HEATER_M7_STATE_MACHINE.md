# Heater / M7 State Machine Specification

## Objective

Recreate the useful parts of Balboa's dual-temperature-sensor heater / flow logic while adding clearer diagnostics.

The controller should infer heating and flow quality using two temperature sensors mounted on opposite sides of the heater.

## Inputs

- Sensor A temperature (heater **inlet** — confirm at first wet heat run)
- Sensor B temperature (heater **outlet** — the sensor that rises first when heat is applied)
- 3.3 V rail reference (ADS1115 A2) for ratiometric conversion
- pump LOW command/state
- pump HIGH command/state
- heater SSR command
- heater safety contactor command
- setpoint (hard-limited 10–40 °C)
- heating mode (NORMAL / SOLAR_PRIORITY)
- current time and time validity (DS3231 / NTP)
- fault state
- optional heater current
- optional pump current

## Derived values

- delta-T = Sensor B − Sensor A (outlet − inlet; positive while heating)
- water temperature (see Temperature sampling)
- water temperature age (seconds since last valid reading)
- sensor agreement
- temperature rise rate
- time since pump start (either speed)
- time since heater enable

## Hardware context

The firmware is not the only heater protection. In hardware:

- heater safety contactor coil and SSR input both pass through spare NO poles of the LOW contactor
- HIGH can only close when LOW is open (NC aux interlock)
- an independent manual-reset high-limit is in the safety contactor coil chain

So firmware cannot energise the heater without LOW pulled in, and cannot run heater with HIGH. Firmware must still sequence correctly so these hardware contacts never break heater load.

---

## Temperature sampling

The M7 sensors are either side of the heater, so they only read tub temperature when water flows through the heater. Flow at **either** pump speed is fine — pump HIGH with the heater locked out still gives valid readings.

| Parameter | Placeholder | Notes |
|---|---|---|
| `SAMPLE_INTERVAL_S` | 1 s | both sensors + rail, every state |
| `TEMP_SETTLE_S` | 60 s | pump (LOW or HIGH) must run this long before readings count as water temperature |
| `POLL_INTERVAL_MIN` | 30 min | time since last valid reading before a poll run starts while pump is stopped |
| `POLL_RUN_S` | 120 s | pump LOW run length for a poll (≥ `TEMP_SETTLE_S` + averaging window) |
| `STALE_DISPLAY_S` | 300 s | after this, UIs show the reading's age |

Rules:

1. Sample continuously in every state, including when the pump is off. Sensor range/open/short checks run on every sample regardless of pump state.
2. **Water temperature** is updated only while the pump has been running (LOW or HIGH) for ≥ `TEMP_SETTLE_S`. Use a short moving average (~10 samples) of the two sensors (or Sensor A when heating, since B is heater-influenced).
3. While pump is stopped, water temperature holds the last valid value and its age increases.
4. If the age exceeds `POLL_INTERVAL_MIN`, enter POLLING (pump LOW for `POLL_RUN_S`). Skip polling while a filter cycle, heating or jets run is already in progress — those runs refresh the reading anyway.
5. Heat demand is evaluated from water temperature only when it is valid and fresh (age < `POLL_INTERVAL_MIN`). A stale reading triggers a poll, not heating.
6. Topside and web UI always show the last valid water temperature; if age > `STALE_DISPLAY_S`, show the age ("updated 12 min ago").
7. While any settle period is running (poll, filter start, heat start, jets), UIs show a small "reading…" indicator so the held value does not look frozen.
8. **After power-up / reboot there is no valid reading.** UIs show dashes (`--.-°`) with "Checking temperature…" and the settle countdown until the first valid reading. The value saved before a power loss is **not** shown — after a long outage it could be badly wrong.

---

## UV / ozone rule

UV / ozone runs whenever the pump runs, at either speed:

- ON after pump LOW or HIGH has been running ≥ `UV_SETTLE_S` (~10 s)
- OFF during LOW↔HIGH changeover dead-time
- OFF whenever the pump is stopped
- applies in every pump-running state: POLLING, FILTERING, CIRCULATING, PREHEAT_CHECK, HEATING, HIGH_SPEED, COOLDOWN
- OFF in SERVICE unless explicitly tested

---

## Filter cycles

Pump LOW runs on configurable daily filter cycles, independent of heat demand.

| Parameter | Default | Notes |
|---|---|---|
| filter cycle 1 | 10:00, 2 h | inside default solar window so heating and filtration share pump time |
| filter cycle 2 | 20:00, 1 h | evening cycle; set duration to 0 to disable |

Rules:
- if heat demand exists during a filter cycle, go straight through CIRCULATING → PREHEAT_CHECK → HEATING (pump already LOW)
- jets HIGH request during a filter cycle is honoured; the remaining filter time resumes on LOW after jets end, if still inside the cycle window
- if time is invalid (no RTC / NTP), run one 2 h filter cycle every 24 h of uptime as a fallback

---

## Heat demand and heating mode

- **NORMAL:** heat whenever water temperature < setpoint − hysteresis (placeholder 0.5 °C).
- **SOLAR_PRIORITY:** heat to setpoint only inside the solar window (default 09:00–15:30). Outside it, heat only if water falls below the economy floor (configurable, default setpoint − 5 °C).
- If time is invalid, SOLAR_PRIORITY behaves as NORMAL and a TIME_INVALID warning is shown.
- Setpoint is clamped to 10–40 °C on every input path (buttons, web, Matter).

---

## Core states

- BOOT
- IDLE
- POLLING
- FILTERING
- CIRCULATING
- PREHEAT_CHECK
- HEATING
- HIGH_SPEED
- COOLDOWN
- FAULT_LOCKOUT
- SERVICE

## BOOT

Outputs:
- heater SSR OFF
- heater safety contactor OFF
- LOW OFF
- HIGH OFF
- UV OFF
- light restored to last user state only after initialization

Actions:
- validate configuration
- validate both heater sensors (range / open / short)
- read RTC; mark time valid/invalid
- initialize watchdog
- clear non-latched transient state
- water temperature marked unknown (`water_valid = false`, `water_c = null`); UIs show `--.-°` until the first valid reading

Transition:
- to POLLING when startup checks pass (to get a fresh temperature)
- to FAULT_LOCKOUT on critical sensor / config failure

## IDLE

Pump OFF, heater OFF, UV OFF.

Possible transitions:
- water temperature age > `POLL_INTERVAL_MIN` -> POLLING
- filter cycle start -> FILTERING
- heat demand (valid, fresh reading) -> CIRCULATING
- jets LOW request -> CIRCULATING (user LOW run; no heat unless demand)
- jets HIGH request -> HIGH_SPEED
- service command -> SERVICE

## POLLING

Actions:
- pump LOW ON
- UV ON after `UV_SETTLE_S`
- wait `TEMP_SETTLE_S`, then update water temperature

Transitions:
- reading valid and heat demand -> PREHEAT_CHECK (pump already LOW)
- `POLL_RUN_S` elapsed, no demand -> IDLE
- jets HIGH request -> HIGH_SPEED
- sensor fault -> FAULT_LOCKOUT

## FILTERING

Actions:
- pump LOW ON for the filter cycle duration
- UV ON after `UV_SETTLE_S`
- water temperature refreshed continuously

Transitions:
- heat demand -> PREHEAT_CHECK
- cycle ends -> IDLE
- jets HIGH request -> HIGH_SPEED
- sensor fault -> FAULT_LOCKOUT

## CIRCULATING

Actions:
- command pump LOW
- confirm HIGH is OFF
- UV ON after `UV_SETTLE_S`
- wait stabilization delay (`TEMP_SETTLE_S`)

Checks:
- both sensors valid
- sensor mismatch within allowed limit
- no over-temp

Transition:
- to PREHEAT_CHECK when stable and heat demand exists
- to FAULT_LOCKOUT on invalid sensor / over-temp
- to IDLE if no heat demand and no user/filter LOW request

## PREHEAT_CHECK

Purpose:
- establish baseline before applying heat

Capture:
- Sensor A
- Sensor B
- baseline delta-T (should be near zero with heater off)

Requirements:
- pump LOW active ≥ `TEMP_SETTLE_S`
- pump HIGH inactive
- valid sensor readings

Transition:
- heater safety contactor ON (hardware permits it only because LOW is pulled in)
- wait contactor close time (~100 ms)
- then HEATING

## HEATING

Requirements:
- LOW active
- HIGH inactive
- both sensors valid
- no over-temp
- delta-T within safe band
- setpoint ≤ 40 °C

Actions:
- heater SSR ON / controlled
- UV ON
- continuously sample temperatures

Trip conditions:
- sensor out of range
- sensor mismatch
- delta-T too high
- outlet rise rate too high
- absolute over-temperature
- no thermal response after timeout (could also indicate a failed LOW spare-pole permissive)
- controller fault

Normal transitions:
- target reached -> COOLDOWN
- HIGH requested -> heater-off sequence, then HIGH_SPEED

## Heater-off sequence (used by every exit from HEATING)

1. SSR OFF
2. wait ≥ 100 ms (SSR releases at next zero crossing)
3. heater safety contactor OFF
4. only now may LOW be dropped or changed

This ensures the LOW spare poles and safety contactor never break heater load in normal operation.

## HIGH_SPEED

Before entry:
1. heater-off sequence (if heating)
2. UV OFF
3. LOW OFF
4. wait contactor dead-time (0.5–1.0 s)
5. HIGH ON
6. UV ON after `UV_SETTLE_S`

Rules:
- heater is prohibited (also impossible in hardware: LOW permissive poles are open)
- LOW and HIGH can never overlap
- temperature sampling continues; water temperature stays current after `TEMP_SETTLE_S`
- jets HIGH has a configurable maximum run time (default 20 min), then returns to previous state

Exit:
- UV OFF
- HIGH OFF
- wait dead-time
- return to LOW (resume FILTERING / heat demand) or IDLE depending on demand

## COOLDOWN

Purpose:
- continue LOW circulation briefly after heater turns OFF to carry heat away from the element

Actions:
- heater OFF (heater-off sequence complete)
- LOW remains ON for configured period (placeholder 60 s)
- UV remains ON

Then:
- IDLE if no demand
- FILTERING if inside a filter cycle
- PREHEAT_CHECK if heat demand returns

## FAULT_LOCKOUT

Immediate actions:
- heater-off sequence
- pump behavior selected according to fault type (e.g. keep LOW running after over-temp to dissipate heat; stop pump on suspected no-flow)
- UV follows pump state
- fault stored in history
- concise fault title/code shown on the GC9A01 topside
- detailed web diagnostic created

Some faults may allow timed retry.
Critical faults remain latched until a **local** manual reset at the topside (e.g. hold Temp+ and Temp− together for 5 s). Latched critical faults cannot be cleared from the web UI or Matter.

## SERVICE

- heater locked out unless a dedicated commissioning mode is entered
- schedules and filter cycles suspended
- outputs only change through explicit, gated service tests

## Suggested initial thresholds

These are placeholders only and must be tuned from real measurements.

- sensor absolute plausibility: configure from OEM sensor curve
- sensor mismatch while heater OFF: small allowable difference
- maximum heater delta-T: tune from observed normal operation
- maximum temperature rise rate: tune from observed normal operation
- no-response timeout: tune from heater power and spa flow
- maximum setpoint: 40 °C (fixed, not tunable)

Do not hard-code final thresholds until wet commissioning data is collected.

## 10 A supply rule

If pump HIGH is active or requested:
- heater command must be OFF
- heater permissive must be removed before HIGH contactor is allowed

This is a hard system rule, not a user option, and is also enforced in hardware by the LOW contactor spare-pole permissive.
