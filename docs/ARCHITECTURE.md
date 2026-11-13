# Architecture

## System overview

```
GC9A01 TFT + 4 buttons (topside, ≤400 mm)
      |  SPI + 4 GPIO inputs
      v
Waveshare ESP32-S3-Relay-6CH  <---- I²C ----  ADS1115 (M7 Sensor A/B, 3.3 V ref)
      |                                      DS3231 RTC
      |
      +--> CH1 --> LOW contactor -----------+
      |             |  (NC aux interlocks   |
      |             |   HIGH coil)          +--> 2-speed pump
      +--> CH2 --> HIGH contactor ----------+
      |             (NC aux interlocks LOW coil)
      |
      |      LOW spare pole 1    high-limit
      +--> CH3 ----[ NO ]----------[ NC ]----> heater safety contactor coil
      |
      |      LOW spare pole 2
      +--> GPIO16 driver ---[ NO ]----------> SSR 24 VDC input
      |
      |    mains: Active -> safety contactor -> SSR -> 2 kW heater
      |
      +--> CH4 --> UV / ozone (only while pump LOW or HIGH runs)
      |
      +--> CH5 --> spa light
      |
      +--> local web UI
      |
      +--> Matter / Google Home
```

## Control layers

### Hardware safety layer
Independent of firmware:
- LOW/HIGH NC auxiliary cross-interlock
- heater safety contactor coil and SSR input both fed through spare NO poles of the LOW contactor, so the heater can only be energised while LOW is physically pulled in (and therefore never with HIGH)
- independent manual-reset heater high-limit in the safety contactor coil chain
- relays default OFF on reset / power-up

### Local safety layer
Runs entirely on the ESP32:
- temperature sensing and timed polling
- M7-style delta-T / rise-rate logic
- pump LOW/HIGH sequencing and dead-time
- low-amperage heater lockout
- UV / ozone follows pump run state
- filter cycles
- fault state machine
- schedules
- watchdog / safe reboot state

### Local user layer
- GC9A01 topside display + Jets / Light / Temp+ / Temp− buttons
- ESP32-hosted web interface

### Remote user layer
- Matter
- Google Home

Remote control only requests states. It never bypasses local safety logic.

## 10 A supply constraint

With a 2 kW heater at roughly 8.3 A, pump HIGH and heater must not operate together. LOW + heater is allowed — the same load pattern the original GS100 ran on the same 10 A outlet.

Required transition to HIGH:

1. heater SSR OFF
2. wait ≥ 100 ms (SSR turns off at the next zero crossing)
3. heater safety contactor OFF
4. UV / ozone OFF
5. LOW OFF (this also removes the hardware heater permissive)
6. wait dead-time (0.5–1.0 s)
7. HIGH ON
8. UV / ozone ON after a short settle delay

Firmware always removes heat before LOW drops, so the LOW spare poles and the safety contactor never break heater load in normal operation.

## Sensor strategy

Primary heater/flow logic uses the two OEM-style temperature sensors positioned either side of the heater.

The controller watches:
- absolute temperature
- A/B agreement
- delta-T
- delta-T rise rate
- time since heat command

Because the sensors only see tub temperature when water flows through the heater, temperature is sampled continuously but treated as the tub temperature only after the pump (LOW or HIGH) has run for a settle time. When the pump is stopped, pump LOW is polled on a timer. See [HEATER_M7_STATE_MACHINE.md](HEATER_M7_STATE_MACHINE.md).

An independent high-limit safety device remains outside the software loop.

## Timekeeping

DS3231 RTC on I²C, corrected from NTP when Wi-Fi is available. Schedules (solar priority, filter cycles) are suspended if no valid time source exists.
