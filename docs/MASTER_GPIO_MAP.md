# Master GPIO / I/O Allocation

This is the project-wide source of truth for ESP32-S3 pin usage. Update this file whenever a GPIO is reassigned.

## Board

Waveshare ESP32-S3-Relay-6CH. The onboard GPIOs below match the Waveshare wiki control-GPIO table. The module is an ESP32-S3-WROOM-1U; sources list N8 or N16 flash, both without PSRAM (check the label on the module can), so GPIO35–37 are usable. There is no onboard RTC (wiki).

## Onboard fixed functions — Waveshare ESP32-S3 Relay 6CH

| GPIO | Function | Status |
|---:|---|---|
| GPIO1 | Relay CH1 | USED |
| GPIO2 | Relay CH2 | USED |
| GPIO41 | Relay CH3 | USED |
| GPIO42 | Relay CH4 | USED |
| GPIO45 | Relay CH5 | USED |
| GPIO46 | Relay CH6 | USED |
| GPIO17 | RS485 TX | RESERVED |
| GPIO18 | RS485 RX | RESERVED |
| GPIO21 | Buzzer | RESERVED |
| GPIO38 | RGB LED | RESERVED |
| GPIO0 | BOOT | RESERVED |

## Reviewed firmware V0.6.2 assignments

| GPIO | Project use | Status |
|---:|---|---|
| GPIO8 | V0.9 default: M7 Sensor A analog in (ADC1_CH7). ADS1115 build: I2C SDA | IMPLEMENTED; wiring unverified |
| GPIO9 | V0.9 default: M7 Sensor B analog in (ADC1_CH8). ADS1115 build: I2C SCL | IMPLEMENTED; wiring unverified |
| GPIO3 | V0.9 optional 3.3 V divider-rail reference, onboard-ADC build (ADC1_CH2, 10k/10k); strapping pin, input only | OPTIONAL, `RAIL_REF_FITTED=false` |
| GPIO10/11 | V0.8 GC9A01 SCLK/MOSI | IMPLEMENTED; electrical validation pending |
| GPIO12/13/14 | V0.8 display CS/DC/RESET | IMPLEMENTED; electrical validation pending |
| GPIO15 | V0.8 external backlight driver ENABLE | DEFAULT OFF; never LED current supply |
| GPIO4/5/6/7 | V0.8 Jets/Light/Temp UP/Temp DOWN | IMPLEMENTED; active LOW inputs |
| GPIO16 | Proposed high-limit status input (`PIN_HIGH_LIMIT_OK`, isolated, 10k pull-down) | PROPOSED; firmware `-1` until wired and tested |
| GPIO47/48 | Proposed DS3231 RTC I2C (onboard-ADC build only; ADS1115 build shares GPIO8/9) | PLANNED |

## Relay channel allocation — V0.6.2 source of truth

| Relay | GPIO | Function |
|---|---:|---|
| CH1 | GPIO1 | Heater safety contactor; physical output inhibited |
| CH2 | GPIO2 | Pump LOW contactor |
| CH3 | GPIO41 | Heater SSR control through dry relay contacts; physical output inhibited |
| CH4 | GPIO42 | Pump HIGH contactor |
| CH5 | GPIO45 | UV / ozone |
| CH6 | GPIO46 | Spa light |

This mapping supersedes the earlier proposal with CH1/CH2 as LOW/HIGH and GPIO16 as SSR. No hardware has been reconfigured by this documentation update. Bench-verify wiring, polarity, boot behavior and contactors before connecting loads. The independent high-limit/feedback pins remain unassigned, and heater commissioning remains blocked.

## Currently free / candidate GPIOs

The following exposed pins are currently unassigned and may be used later after bench verification:

- GPIO35
- GPIO36
- GPIO37
- GPIO39
- GPIO40

Do not allocate any of these permanently without first checking:
- Waveshare schematic
- boot/strap behavior
- conflicts with onboard peripherals
- voltage compatibility

## Rules

1. No user-interface code may directly control mains outputs.
2. Matter, web UI and custom topside handlers only submit requests to the safety state machine.
3. GPIO assignments in firmware must be defined in one central config file.
4. Any pin change must be reflected here before firmware is merged.

V0.8 replaces the abandoned VL260 proposal. Topside pins are centralized in `firmware/libraries/SpaTopside/src/TopsideConfig.h`. See `TOPSIDE_V08_HARDWARE.md` for wiring, module-current and verification requirements. No relay allocations changed.

## V0.9 notes

- Relay map unchanged from V0.6.2 / V0.8 (CH1 safety contactor, CH2 LOW, CH3 SSR supply, CH4 HIGH, CH5 UV, CH6 light). This is the correct map; the older proposal on `main` (CH1 LOW, CH2 HIGH, …, GPIO16 SSR) is superseded.
- Display pins are as implemented in `TopsideConfig.h` (SCLK 10, MOSI 11, CS 12, DC 13, RST 14, BL 15). They are not moved: the GPIO matrix is fast enough for a 240×240 panel.
- Only ADC1 (GPIO1–10) is usable while Wi-Fi is active. In the onboard-ADC build, GPIO3 is the only spare ADC1 pin, so the 24 V monitor is available in the ADS1115 build only (A3).
