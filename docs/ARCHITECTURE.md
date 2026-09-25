# Architecture

## System overview

```
MVP260 topside
      |
      v
MVP260 interface
      |
      v
Waveshare ESP32-S3 6CH
      |
      +--> Pump LOW relay --> LOW contactor ----+
      |                                         |
      +--> Pump HIGH relay -> HIGH contactor ---+--> 2-speed pump
      |
      +--> Heater logic --> safety contactor --> SSR --> 2 kW heater
      |
      +--> UV / ozone
      |
      +--> spa light
      |
      +--> local web UI
      |
      +--> Matter / Google Home
```

## Control layers

### Local safety layer
Runs entirely on the ESP32:
- temperature sensing
- M7-style delta-T / rise-rate logic
- pump LOW/HIGH interlock
- low-amperage heater lockout
- fault state machine
- schedules
- watchdog / safe reboot state

### Local user layer
- MVP260 topside
- ESP32-hosted web interface

### Remote user layer
- Matter
- Google Home

Remote control only requests states. It never bypasses local safety logic.

## 10 A supply constraint

With a 2 kW heater at roughly 8.3 A, pump HIGH and heater must not operate together.

Required transition to HIGH:

1. heater SSR OFF
2. verify heater command OFF
3. drop heater permissive if necessary
4. LOW OFF
5. wait dead-time
6. HIGH ON

## Sensor strategy

Primary heater/flow logic uses the two OEM-style temperature sensors positioned either side of the heater.

The controller watches:
- absolute temperature
- A/B agreement
- delta-T
- delta-T rise rate
- time since heat command

An independent high-limit safety device remains outside the software loop.
