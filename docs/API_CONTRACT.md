# Fisher Spa Web API / Data Contract

## Design principle

The web app consumes a small JSON API from the ESP32.

The API exposes controller state and accepts safe high-level requests.

It does not expose unrestricted GPIO or relay control.

## Authentication

- `GET` endpoints are readable on the LAN without a token.
- Every state-changing `POST` requires header `Authorization: Bearer <token>`.
- The token is generated on first boot and shown as a QR code on the topside Setup screen (hold Jets + Light 3 s). It can be regenerated from the Setup screen. See [PROVISIONING_AND_UPDATES.md](PROVISIONING_AND_UPDATES.md).
- Missing/invalid token → `401`. Repeated failures are rate-limited.
- Demo mode needs no token and never touches outputs.

## GET /api/state

Example response:

```json
{
  "controller": {
    "online": true,
    "uptime_s": 48231,
    "firmware": "0.1.0-dev",
    "build": "local",
    "reset_reason": "power_on",
    "watchdog_ok": true
  },
  "temperature": {
    "water_c": 36.8,
    "water_valid": true,
    "water_age_s": 42,
    "settling": false,
    "settle_remaining_s": null,
    "target_c": 38.0,
    "target_min_c": 10.0,
    "target_max_c": 40.0,
    "sensor_a_c": 36.7,
    "sensor_b_c": 37.1,
    "delta_t_c": 0.4,
    "rise_rate_c_min": 0.12
  },
  "pump": {
    "requested": "LOW",
    "actual": "LOW"
  },
  "heater": {
    "demand": true,
    "ssr_command": true,
    "safety_contactor": true,
    "allowed": true
  },
  "uv": {
    "on": true,
    "reason": "PUMP_RUNNING"
  },
  "circulation": {
    "activity": "HEATING",
    "next_poll_in_s": null,
    "filter_cycle_active": false
  },
  "light": {
    "on": false,
    "program": 1,
    "program_count": 5,
    "programs": [
      {"n": 1, "name": "White", "swatch": "#ffffff"},
      {"n": 2, "name": "Blue", "swatch": "#2060ff"},
      {"n": 3, "name": "Green", "swatch": "#20c040"},
      {"n": 4, "name": "Red", "swatch": "#ff3030"},
      {"n": 5, "name": "Colour cycle (slow)", "swatch": null}
    ],
    "auto_off_remaining_s": null
  },
  "priming": {
    "active": false,
    "remaining_s": null
  },
  "input_lockout_ms": 0,
  "m7": {
    "status": "OK",
    "flow_ok": true
  },
  "mode": {
    "heating": "SOLAR_PRIORITY",
    "service": false,
    "demo": false
  },
  "schedule": {
    "enabled": true,
    "start": "09:00",
    "end": "15:30",
    "days": [1,2,3,4,5,6,7],
    "economy_floor_c": 33.0
  },
  "filter_cycles": [
    {"start": "10:00", "duration_min": 120},
    {"start": "20:00", "duration_min": 60}
  ],
  "time": {
    "valid": true,
    "source": "NTP",
    "local": "2026-11-13T14:30:00+11:00"
  },
  "fault": {
    "active": false,
    "panel_code": "",
    "id": "",
    "title": ""
  },
  "network": {
    "wifi_connected": true,
    "rssi_dbm": -55,
    "matter_paired": true
  }
}
```

## POST /api/target

Request:
```json
{"target_c":38.0}
```

Firmware clamps / rejects values outside 10–40 °C (`400` with the allowed range). 40 °C is a hard maximum, not a setting.

## POST /api/jets

Request:
```json
{"requested":"HIGH"}
```

Allowed values:
- OFF
- LOW
- HIGH

Firmware performs all heater shutdown / dead-time / interlock sequencing.

## POST /api/light

```json
{"on":true}
```

```json
{"on":true, "program":4}
```

`program` (1…`program_count`) is optional. Firmware sends the power-interruption step pulses needed to reach it. Auto-off after 40 min.

## Input lockout

`POST /api/jets` and `POST /api/light` are ignored for 2 s after any pump or light state change, from any source. They are not queued. Response: `409` with

```json
{"error":"INPUT_LOCKOUT","lockout_remaining_ms":1400}
```

## POST /api/priming/end

Ends priming early (same as pressing Temp+ / Temp− on the topside during priming). Requires `"confirm": true`.

## Install self-test

See [COMMISSIONING_SELF_TEST.md](COMMISSIONING_SELF_TEST.md).

- `GET /api/commissioning`: overall status (`PENDING`, `PART_A_RUNNING`, `PART_B_RUNNING`, `COMPLETE`), per-check results and values, current guided prompt if any, suggested thresholds
- `POST /api/commissioning/answer`: `{"step":"B5","answer":"YES"}`; answers `YES`, `NO`, `REPEAT`, `SKIP`. For the B6 colour decode: `{"step":"B6","answer":"COLOUR","value":"Blue"}` or `{"step":"B6","answer":"BACK_TO_FIRST"}`. Token required
- `POST /api/light/programs`: rename decoded programs, e.g. `[{"n":2,"name":"Ocean blue"}]`; token required
- `POST /api/commissioning/start-guided`: starts Part B; token + confirm required
- `POST /api/commissioning/accept-thresholds`: applies the suggested trip thresholds; token + confirm required
- `POST /api/commissioning/rerun`: Service mode only; token + confirm required

## Firmware update

See [PROVISIONING_AND_UPDATES.md](PROVISIONING_AND_UPDATES.md).

- `GET /api/firmware` — running version, build, partition, previous version available for rollback, last update result
- `POST /api/firmware/update` — multipart upload of a firmware `.bin`; token + `"confirm": true` required; rejected (`409`) while heating, priming or in fault lockout
- `POST /api/firmware/rollback` — boot the previous firmware; token + confirm required

## POST /api/mode

```json
{"heating":"SOLAR_PRIORITY"}
```

Allowed values initially:
- NORMAL
- SOLAR_PRIORITY

## POST /api/schedule

Example:
```json
{
  "enabled": true,
  "start": "09:00",
  "end": "15:30",
  "days": [1,2,3,4,5,6,7]
}
```

## POST /api/filter-cycles

```json
[
  {"start": "10:00", "duration_min": 120},
  {"start": "20:00", "duration_min": 60}
]
```

Up to 2 cycles. `duration_min` 0–240; 0 disables a cycle.

## Field notes

- `temperature.water_c` is the last **valid** water temperature (pump has run ≥ settle time). `water_age_s` is its age. UIs show the age once it exceeds 300 s.
- After power-up, before the first valid reading: `water_valid: false`, `water_c: null`, `water_age_s: null`. UIs show `--.-°`.
- `temperature.settling` (bool) and `temperature.settle_remaining_s` (int or null) report an in-progress settle period, for the "reading…" indicator and the power-up countdown.
- `uv.reason`: `PUMP_RUNNING`, `PUMP_STOPPED`, `SPEED_CHANGEOVER`, `SETTLING`, `SERVICE`, `FAULT`.
- `circulation.activity`: `IDLE`, `POLLING`, `FILTERING`, `HEATING`, `COOLDOWN`, `JETS_LOW`, `JETS_HIGH`.
- `time.source`: `NTP`, `RTC`, `NONE`. When `valid` is false, schedules are suspended.

## GET /api/faults

Return a bounded list of recent fault/event records.

Suggested fault object:

```json
{
  "timestamp":"2026-11-05T10:43:12+11:00",
  "panel_code":"Sn",
  "id":"SENSOR_B_RANGE",
  "title":"Sensor B out of range",
  "severity":"FAULT",
  "snapshot":{
    "sensor_a_c":36.8,
    "sensor_b_c":null,
    "delta_t_c":null,
    "pump":"LOW",
    "heater":false
  },
  "probable_causes":[
    "Sensor unplugged",
    "Sensor failed",
    "Damaged cable",
    "Connector corrosion"
  ],
  "recommended_checks":[
    "Inspect Sensor B connector",
    "Measure sensor resistance",
    "Inspect wiring"
  ]
}
```

## GET /api/diagnostics

Service data:
- raw ADC
- calculated thermistor resistance
- GPIO/relay logical state
- permissives
- watchdog state
- heap/free memory
- network status

## POST /api/service-mode

Explicitly enable/disable persistent service mode.

## POST /api/fault-reset

Attempts to clear only faults that firmware marks **remote-resettable** (warnings and retryable faults). Latched critical faults (e.g. OVER_TEMP) return `409` with `"local_reset_required": true` — they can only be reset at the topside.

## Security / validation

- All commands validated server-side.
- No endpoint may directly set a physical output.
- Reject malformed JSON.
- Bound numeric values.
- Rate-limit repeated commands.
- Never let the browser become the source of truth.
- All `POST` endpoints require the bearer token.
- `/api/service-mode` and `/api/fault-reset` additionally require a confirmation field (`"confirm": true`).
