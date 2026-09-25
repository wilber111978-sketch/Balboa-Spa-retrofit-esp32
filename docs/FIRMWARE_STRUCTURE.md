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

    sensors/
      heater_sensors.cpp
      heater_sensors.h
      current_monitor.cpp
      current_monitor.h

    panel/
      mvp260.cpp
      mvp260.h

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
```

## Architectural rules

### Safety first
All hardware outputs should be controlled through a single safety-aware state machine.

No web / Matter / panel handler should directly energize:
- heater SSR
- heater safety contactor
- pump contactors

Instead, interfaces submit requests to the controller.

### Single source of truth
Maintain one central spa state model containing:
- current mode
- target temperature
- measured temperatures
- pump state
- heater state
- fault state
- safety permissives

### Fault manager
Responsible for:
- creating fault events
- mapping internal triggers to MVP260 codes
- storing probable causes
- storing recommended checks
- retry / latch policy

### Event log
Use a compact ring-buffer / persistent log for:
- faults
- major state transitions
- heater cycles
- manual resets

### Matter
Matter should expose only safe high-level controls:
- setpoint
- jets
- light
- basic status

Matter must never bypass the state machine.

### Web UI
The web UI can expose deeper diagnostics, but still must route all commands through the same safety layer.

### Testing
State-machine and fault logic should be unit-tested before mains commissioning.
