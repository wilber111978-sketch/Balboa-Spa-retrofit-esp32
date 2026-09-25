# Heater / M7 State Machine Specification

## Objective

Recreate the useful parts of Balboa's dual-temperature-sensor heater / flow logic while adding clearer diagnostics.

The controller should infer heating and flow quality using two temperature sensors mounted on opposite sides of the heater.

## Inputs

- Sensor A temperature
- Sensor B temperature
- pump LOW command/state
- pump HIGH command/state
- heater SSR command
- heater safety contactor state
- setpoint
- fault state
- optional heater current
- optional pump current

## Derived values

- delta-T = Sensor B - Sensor A
- absolute temperature
- sensor agreement
- temperature rise rate
- time since pump start
- time since heater enable

## Core states

- BOOT
- IDLE
- CIRCULATING
- PREHEAT_CHECK
- HEATING
- HIGH_SPEED
- COOLDOWN
- FAULT_LOCKOUT
- SERVICE

## BOOT

Outputs:
- heater OFF
- LOW OFF
- HIGH OFF
- UV OFF or restored only after initialization
- light safe/default

Actions:
- validate configuration
- validate both heater sensors
- initialize watchdog
- clear non-latched transient state

Transition:
- to IDLE when startup checks pass
- to FAULT_LOCKOUT on critical sensor / config failure

## IDLE

Heater OFF.

Possible transitions:
- heat demand -> CIRCULATING
- jets HIGH request -> HIGH_SPEED
- service command -> SERVICE

## CIRCULATING

Actions:
- command pump LOW
- confirm HIGH is OFF
- wait stabilization delay

Checks:
- both sensors valid
- sensor mismatch within allowed limit
- no over-temp

Transition:
- to PREHEAT_CHECK when stable
- to FAULT_LOCKOUT on invalid sensor / over-temp
- to IDLE if heat demand disappears

## PREHEAT_CHECK

Purpose:
- establish baseline before applying heat

Capture:
- Sensor A
- Sensor B
- baseline delta-T

Requirements:
- pump LOW active
- pump HIGH inactive
- valid sensor readings

Transition:
- enable heater safety permissive
- then HEATING

## HEATING

Requirements:
- LOW active
- HIGH inactive
- both sensors valid
- no over-temp
- delta-T within safe band
- safety contactor allowed

Actions:
- heater SSR ON / controlled
- continuously sample temperatures

Trip conditions:
- sensor out of range
- sensor mismatch
- delta-T too high
- outlet rise rate too high
- absolute over-temperature
- HIGH speed request
- controller fault

Normal transitions:
- target reached -> COOLDOWN or IDLE
- HIGH requested -> heater OFF then HIGH_SPEED

## HIGH_SPEED

Before entry:
1. heater SSR OFF
2. remove heater permissive as required
3. LOW OFF
4. wait contactor dead-time
5. HIGH ON

Rules:
- heater is prohibited
- LOW and HIGH can never overlap

Exit:
- HIGH OFF
- wait dead-time
- return to LOW / IDLE depending on demand

## COOLDOWN

Purpose:
- continue LOW circulation briefly after heater turns OFF

Actions:
- heater OFF
- LOW remains ON for configured period

Then:
- IDLE if no demand
- PREHEAT_CHECK if heat demand returns

## FAULT_LOCKOUT

Immediate actions:
- heater SSR OFF
- heater safety contactor de-energized
- pump behavior selected according to fault type
- fault stored in history
- MVP260 short fault shown
- detailed web diagnostic created

Some faults may allow timed retry.
Critical faults remain latched until manual reset.

## Suggested initial thresholds

These are placeholders only and must be tuned from real measurements.

- sensor absolute plausibility: configure from OEM sensor curve
- sensor mismatch while heater OFF: small allowable difference
- maximum heater delta-T: tune from observed normal operation
- maximum temperature rise rate: tune from observed normal operation
- no-response timeout: tune from heater power and spa flow

Do not hard-code final thresholds until wet commissioning data is collected.

## 10 A supply rule

If pump HIGH is active or requested:
- heater command must be OFF
- heater permissive must be removed before HIGH contactor is allowed

This is a hard system rule, not a user option.
