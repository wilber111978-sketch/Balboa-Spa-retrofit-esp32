# Fisher Spa Web App — UI / UX Specification

## Purpose

Build the local web interface that will be hosted directly by the ESP32-S3 and used as the primary advanced control and diagnostics page for the Fisher 3-person spa retrofit.

The visual reference is the generated phone mockup from the project conversation.

The UI must feel like a modern smart-home app, but the ESP32 remains the controller and the UI never directly bypasses the safety state machine.

## Primary goals

- Fast, readable, mobile-first interface.
- Works well on Android phone and desktop browser.
- Clear separation between normal controls and service diagnostics.
- Full operation on LAN without any cloud service.
- Google Home / Matter remains a separate remote-control path.
- No direct relay toggles exposed to normal users.
- Faults should be descriptive and actionable.

## Main screen

### Header
- Title: Fisher Spa
- Subtitle: 3 Person Spa
- connection indicator
- Matter / Google Home status
- settings icon

### Main temperature card
Show:
- current water temperature (last valid reading)
- reading age when older than 5 min, e.g. "updated 12 min ago" — never present a stale value as live
- "reading…" indicator while a new reading is settling
- after controller power-up / reboot, before the first valid reading: `--.-°` with "Checking temperature…" and the settle countdown
- target temperature
- heating state
- local/remote connection state

### Status tiles
Five compact tiles:
- Pump: OFF / LOW / HIGH
- Heater: OFF / ON / LOCKED
- UV: OFF / ON (show reason when off, e.g. "pump stopped")
- M7 Flow: OK / CHECK / FAULT
- Fault: NONE / active code

### Diagnostics summary
Show:
- Sensor A
- Sensor B
- Heater ΔT
- optional heater rise rate
- optional heater current
- optional pump current

### Temperature control
- minus button
- target temperature
- plus button
- steps should follow firmware settings
- range 10–40 °C; + button disabled at 40 °C
- disable when service/fault state forbids setpoint changes

### Jets control
Segmented control:
- OFF
- LOW
- HIGH

The UI submits a requested state only. Firmware performs all sequencing and interlocks.

### Light
- on/off toggle
- colour / program picker (1…program count), shown while the light is on
- auto-off time remaining (40 min timer)

### Priming banner
After controller power-up, show a banner across all screens: "Priming — heater off, 3:12 remaining", with an "End priming" button (confirm).

### Input lockout
After a jets or light change, those controls show a brief 2 s disabled state. Taps during it do nothing (no queueing).

### Heating mode
At minimum:
- Normal
- Solar Priority

### Schedule
Show active schedule, for example:
- 09:00–15:30
- days enabled
- enabled/disabled state
- economy floor temperature
- next filter cycle
- warning if time is invalid (schedules suspended)

### Bottom navigation
- Home
- History
- Diagnostics
- Settings

## History screen

Show chronological events:
- faults
- heater starts/stops
- pump speed changes
- service mode entry/exit
- reboots
- watchdog recovery
- settings changes

Each fault entry should be tappable to open full detail.

## Diagnostics screen

### Live values
- Sensor A
- Sensor B
- delta-T
- temperature rise rate
- pump state
- heater request
- safety contactor state
- SSR command
- UV state
- controller uptime
- Wi-Fi RSSI
- last reset reason
- watchdog status
- Matter status

### Active fault card
Show:
- OEM-style short code where applicable
- internal fault ID
- plain-English title
- trigger condition
- timestamp
- probable causes
- recommended checks
- reset/retry status
- for latched critical faults: "Reset at the spa — hold Temp+ and Temp− for 5 s" (no remote reset button)

### Raw/service values
Service-mode only:
- raw ADC values
- thermistor resistance
- relay logical states
- GPIO logical states
- safety permissives

## Settings screen

Include:
- target temperature limits
- temperature units
- heating mode
- solar-priority schedule and economy floor
- filter cycles (2 × start time + duration)
- UV / ozone behavior (read-only note: runs whenever the pump runs)
- time source / RTC status
- web app pairing token (entered once, stored in the browser)
- display brightness or theme if useful
- Wi-Fi configuration
- Matter pairing/reset
- fault history clear
- service mode
- firmware version / build info
- firmware update: upload file, progress, result, "roll back to previous version" (see [PROVISIONING_AND_UPDATES.md](PROVISIONING_AND_UPDATES.md))
- light: number of programs / step timing (service only)

Dangerous operations must require confirmation.

## Service mode

Service mode must be explicit and persistent until exited.

When active:
- heater remains locked out unless a dedicated commissioning mode is entered
- normal schedules are suspended
- service status is obvious on every screen
- direct hardware tests, if later implemented, must be separately gated

## Demo mode

The UI must be fully usable against simulated controller data.

Initial demo scenarios:
- normal idle
- normal heating
- target reached
- jets high
- sensor A fault
- sensor B fault
- sensor mismatch
- low-flow / high delta-T
- heater no-response
- over-temperature
- Wi-Fi disconnected
- watchdog reboot recovered
- pump stopped, temperature reading 25 min old (poll pending)
- temperature poll in progress
- filter cycle running
- time invalid (no RTC / NTP)
- first reading after power-up (`--.-°` with countdown)
- priming in progress
- light on, program 4, 12 min remaining
- input lockout active after a jets change

Demo mode must never energize physical outputs.

## Visual style

Use the generated phone concept as the reference:
- dark charcoal background
- rounded cards
- subtle blue accents
- orange/yellow for active heating
- green for healthy states
- warning colors only for real warnings/faults
- high contrast and readable typography
- avoid clutter

## Mobile-first layout

Target first:
- 360–430 px wide phone viewport

Then adapt:
- tablet
- desktop

No horizontal scrolling.

## Accessibility

- minimum practical touch target ~44 px
- never use color alone to communicate a fault
- visible text labels for all key states
- controls must show disabled state clearly

## Safety UX rule

The web UI is never allowed to:
- directly energize heater SSR
- directly energize safety contactor
- directly energize LOW/HIGH contactors
- bypass fault lockout

It only submits high-level requests to firmware.
