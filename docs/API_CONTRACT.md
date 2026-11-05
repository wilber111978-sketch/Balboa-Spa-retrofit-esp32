# Fisher Spa Web API / Data Contract

## Design principle

The web app consumes a small JSON API from the ESP32.

The API exposes controller state and accepts safe high-level requests.

It does not expose unrestricted GPIO or relay control.

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
    "target_c": 38.0,
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
    "on": true
  },
  "light": {
    "on": false
  },
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
    "days": [1,2,3,4,5,6,7]
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

Firmware validates allowed range before applying.

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

Attempts to clear only faults that firmware marks resettable.

## Security / validation

- All commands validated server-side.
- No endpoint may directly set a physical output.
- Reject malformed JSON.
- Bound numeric values.
- Rate-limit repeated commands.
- Never let the browser become the source of truth.
