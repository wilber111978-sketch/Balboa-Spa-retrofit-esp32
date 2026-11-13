# Work Handoff — Web App Development

## Mission

Implement the first functional Fisher Spa web application in this repository using the existing project documents as authoritative context.

Do not redesign the controller architecture.

The application should be a mobile-first local web UI intended to be served from the Waveshare ESP32-S3.

## Read first

Read these files before changing code:

- README.md
- docs/ARCHITECTURE.md
- docs/MASTER_GPIO_MAP.md
- docs/HARDWARE_INTERFACE.md
- docs/HEATER_M7_STATE_MACHINE.md
- docs/M7_SENSOR_INTERFACE.md
- docs/FAULTS.md
- docs/APP_UI_SPEC.md
- docs/API_CONTRACT.md
- docs/BOM.md
- docs/HARDWARE_INVENTORY.md
- docs/TOPSIDE_GC9A01.md

Treat the safety architecture in those files as fixed unless a contradiction or dangerous defect is found.

## Important confirmed hardware

- Fisher 3-person spa
- OEM Balboa GS100
- original Balboa VL260 topside panel is being retired
- replacement topside: 1.28-inch round GC9A01 240×240 SPI TFT
- four waterproof momentary buttons: Jets, Light, Temp +, Temp -
- single 2-speed pump
- 2 kW heater
- UV / ozone
- spa light
- 10 A 230–240 V supply
- Waveshare ESP32-S3 Relay-6CH
- two 24 VDC pump contactors
- zero-cross SSR for heater switching
- separate 24 VDC heater safety contactor
- two OEM M7-style temperature sensors

## First development objective

Build the app in **demo/simulator mode first**.

Do not wait for live hardware integration.

The app should be navigable and functional using synthetic state data that matches the API contract.

## Recommended implementation

Prefer a lightweight static SPA suitable for ESP32 flash:
- HTML
- CSS
- vanilla JS or very small dependency footprint

Avoid a heavy runtime framework unless there is a clear measurable advantage.

The production output must be usable as static files under:
```
firmware/data/
  index.html
  styles.css
  app.js
```

If a build tool is used, commit both source and clear build instructions, and keep generated assets small.

## Required screens

### Home
Match the generated phone mockup:
- Fisher Spa header
- current temp
- target temp
- Heating status
- Pump / Heater / UV / M7 Flow / Fault tiles
- Sensor A / Sensor B / delta-T summary
- target +/- control
- Jets OFF/LOW/HIGH
- Light toggle
- Normal / Solar Priority
- schedule summary
- bottom nav

### History
- event list
- fault list
- tap event for details

### Diagnostics
- live sensor data
- controller/network state
- active fault detail
- probable causes
- recommended checks

### Settings
- schedule editor (solar window + economy floor)
- filter cycle editor
- heating mode
- service mode
- Matter status placeholder
- firmware/build info

## Demo scenarios

Implement selectable demo scenarios:
1. normal idle
2. heating normally
3. setpoint reached
4. jets HIGH
5. Sensor A fault
6. Sensor B fault
7. sensor mismatch
8. low-flow / excessive delta-T
9. heater no-response
10. over-temperature
11. Wi-Fi disconnected
12. watchdog recovered
13. pump stopped, stale temperature (poll pending)
14. temperature poll in progress
15. filter cycle running
16. time invalid (schedules suspended)
17. first reading after power-up (`--.-°` with countdown)
18. priming in progress
19. light on with colour program and auto-off countdown
20. input lockout after a jets change
21. install test pending
22. install test guided prompt in progress
23. install test complete with suggested thresholds

Scenario changes should update the complete UI coherently.

## Safety constraints

Normal UI controls may submit only:
- requested target temperature
- requested jets state
- light state
- heating mode
- schedule
- safe service actions

Do not implement direct relay / GPIO controls in the normal UI.

Any future commissioning output test must be behind a separate explicit commissioning mode.

## Data architecture

Create one central front-end state object matching docs/API_CONTRACT.md.

Use a service abstraction so the UI can switch between:
- DemoSpaService
- HttpSpaService

This lets the same UI run immediately in demo mode and later against the ESP32 API.

## Offline / error behavior

UI must clearly show:
- controller offline
- stale data
- Wi-Fi disconnected
- API command rejected

Never silently assume a command succeeded.

## Styling

Use the generated mockup as visual direction:
- dark premium smart-home interface
- blue accents
- green healthy states
- amber/orange heating
- red only for actual fault
- rounded cards
- high readability

Do not attempt pixel-perfect recreation if it harms usability.

## Tests

Add lightweight tests for:
- state rendering
- jets request mapping
- target bounds
- disabled controls during fault/service states
- fault detail rendering
- stale/offline UI
- demo scenario transitions

If no JS test framework exists, use a minimal one appropriate to the implementation.

## Acceptance criteria

The first milestone is complete when:

- app loads locally without a backend
- all four screens work
- demo scenarios work
- layout is usable on phone width
- no console errors
- no control directly maps to relay/GPIO
- state/service abstraction exists
- API integration points are implemented or stubbed cleanly
- README contains exact run/build instructions
- screenshots of Home and Diagnostics are included in the PR or repo docs
- no changes are made to established relay assignments or safety rules

## Do not spend time on yet

- live Matter pairing
- real M7 ADC integration
- real TFT/topside hardware integration
- real mains commissioning
- pH/ORP
- dosing
- cloud relay
- OTA update system

Those are later stages.

## Deliverable

Commit the app implementation to this repository in a focused branch and leave a concise implementation note describing:
- files added
- how to run demo mode
- how the service abstraction works
- any deviations from this brief
- remaining hardware integration tasks
