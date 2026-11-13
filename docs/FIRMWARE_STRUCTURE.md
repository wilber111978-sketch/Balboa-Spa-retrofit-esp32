# Firmware Structure

Recommended project layout:

```
firmware/
  src/
    main.cpp
    config.h

    control/
      spa_state_machine.cpp
      spa_state_machine.h
      heater_control.cpp
      heater_control.h
      pump_control.cpp
      pump_control.h
      safety_manager.cpp
      safety_manager.h
      circulation.cpp        # temperature polling + filter cycles
      circulation.h
      uv_ozone.cpp           # UV/ozone follows pump run state
      uv_ozone.h
      heating_schedule.cpp   # NORMAL / SOLAR_PRIORITY / economy floor
      heating_schedule.h

    sensors/
      heater_sensors.cpp     # ADS1115 A0/A1 + A2 rail ratiometric, table interpolation
      heater_sensors.h
      water_temperature.cpp  # validity, settle time, age
      water_temperature.h
      current_monitor.cpp
      current_monitor.h

    outputs/
      light_control.cpp      # 40 min timer, RGB program stepping via CH5
      light_control.h
      input_lockout.cpp      # 2 s pump/light change lockout, all sources
      input_lockout.h

    system/
      provisioning.cpp       # Matter Wi-Fi provisioning, fallback setup AP, token
      provisioning.h
      ota_update.cpp         # app upload, rollback, self-test
      ota_update.h
      persistence.cpp        # NVS settings, write debounce, latched-fault store
      persistence.h

    time/
      rtc_ds3231.cpp
      rtc_ds3231.h
      time_service.cpp       # DS3231 + NTP, time-valid flag
      time_service.h

    topside/
      gc9a01_display.cpp
      gc9a01_display.h
      topside_buttons.cpp
      topside_buttons.h
      topside_ui.cpp
      topside_ui.h

    diagnostics/
      fault_manager.cpp
      fault_manager.h
      fault_catalog.cpp
      fault_catalog.h
      event_log.cpp
      event_log.h

    web/
      web_server.cpp
      web_server.h
      api.cpp
      api.h

    matter/
      matter_bridge.cpp
      matter_bridge.h

    storage/
      settings.cpp
      settings.h

  data/
    index.html
    app.js
    styles.css

  test/
    test_state_machine.cpp
    test_fault_logic.cpp
    test_sensor_validation.cpp
    test_topside_buttons.cpp
    test_topside_ui.cpp
    test_circulation_polling.cpp
    test_uv_ozone_rule.cpp
    test_heater_off_sequence.cpp
```

## Architectural rules

### Safety first
All hardware outputs are controlled through one safety-aware state machine.

No web / Matter / TFT/button handler may directly energize:
- heater SSR
- heater safety contactor
- pump contactors

Interfaces submit requests to the controller.

### New topside
The old VL260 protocol decoder is no longer part of the production firmware.

The replacement local UI is:
- GC9A01 240×240 TFT
- Jets button
- Light button
- Temperature + button
- Temperature - button

Button handling must include:
- debounce
- stuck-button detection
- optional long-press behavior
- Temp+ and Temp− held together for 5 s = local reset of latched faults (the only path that can clear them)
- no blocking delays

The display renders controller state only; it is not the source of truth.

### Single source of truth
Maintain one central spa state model containing:
- current mode
- target temperature
- measured temperatures
- water temperature validity and age
- time validity / source
- filter-cycle status
- pump state
- heater state
- light state
- UV state
- fault state
- safety permissives

### Fault manager
Responsible for:
- creating fault events
- assigning concise local display codes/titles
- storing probable causes
- storing recommended checks
- retry / latch policy

### Event log
Use a compact ring-buffer / persistent log for:
- faults
- major state transitions
- heater cycles
- manual resets
- controller reboots

### Hard limits (compile-time constants, not settings)
- setpoint maximum 40 °C
- LOW/HIGH dead-time minimum
- heater-off sequence before any pump change
- UV/ozone only while pump runs

### Matter
Matter exposes only safe high-level controls:
- setpoint (clamped 10–40 °C)
- jets
- light
- basic status

### Web UI
The web UI exposes deeper diagnostics but routes all commands through the same safety layer. State-changing endpoints require an auth token (see [API_CONTRACT.md](API_CONTRACT.md)).

### Testing
State-machine, topside-button and fault logic should be unit-tested before mains commissioning.
