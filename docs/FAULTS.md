# Fault and Diagnostics Specification

The project will preserve short OEM-style panel faults where practical, but the web UI will expose the actual trigger condition and useful troubleshooting information.

## Fault event record

Every stored fault should include:

- timestamp
- panel fault code
- internal trigger ID
- human-readable title
- setpoint
- Sensor A
- Sensor B
- delta-T
- pump command
- heater command
- UV state
- retry count
- trip threshold
- elapsed time before trip
- probable causes
- recommended checks
- clear / reset time

## Initial trigger set

| Internal ID | Human-readable message | Typical panel presentation |
|---|---|---|
| SENSOR_A_RANGE | Sensor A out of valid range | Sn |
| SENSOR_B_RANGE | Sensor B out of valid range | Sn |
| SENSOR_MISMATCH | Heater sensors disagree abnormally | Sn |
| DELTA_T_HIGH | Heater temperature differential too high | dr / flow-style |
| NO_FLOW_SUSPECTED | Heater temperature rising too quickly; low flow suspected | dr / flow-style |
| HEAT_NO_RESPONSE | Heater commanded but no thermal response detected | service diagnostic |
| OVER_TEMP | Heater / water over-temperature | OH-style |
| PUMP_SPEED_CONFLICT | LOW and HIGH requested together | service diagnostic |
| HIGH_SPEED_HEATER_LOCKOUT | Heater request blocked because pump HIGH is active | normal protective state |
| SSR_OVER_TEMP | Heater SSR temperature too high | service diagnostic |

## Example: SENSOR_B_RANGE

**Title:** Sensor B out of range / invalid reading

Captured:
- Sensor A: 36.8 °C
- Sensor B: invalid
- Pump: LOW
- Heater: OFF

Probable causes:
- unplugged sensor
- failed sensor
- damaged cable
- connector corrosion
- input circuit fault

Recommended checks:
1. inspect connector and cable
2. measure sensor resistance
3. swap A/B sensor inputs if appropriate
4. inspect input circuitry

## Example: HEAT_NO_RESPONSE

**Title:** Heating requested but insufficient temperature change detected

Trigger context:
- pump LOW confirmed
- heater commanded ON
- sensor readings valid
- delta-T / rise rate remains below configured threshold after timeout

Probable causes:
- SSR failed open
- safety contactor not closing
- failed heater element
- open circuit
- wiring fault

With future current sensing, this can become more specific:
- heater commanded ON + 0 A = likely open switching/load path
- heater commanded OFF + current present = possible stuck SSR/contact failure

## Example: NO_FLOW_SUSPECTED

**Title:** Heater outlet temperature rising too quickly

Probable causes:
- blocked filter
- air lock
- low water level
- weak circulation
- closed/restricted plumbing
- pump impeller problem

Immediate action:
- heater OFF
- safety permissive removed
- fault recorded
- controlled retry only if configured and safe
