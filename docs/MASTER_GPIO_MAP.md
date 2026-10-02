# Master GPIO / I/O Allocation

This is the project-wide source of truth for ESP32-S3 pin usage. Update this file whenever a GPIO is reassigned.

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

## Project assignments

| GPIO | Project use | Status |
|---:|---|---|
| GPIO8 | I2C SDA — ADS1115 / sensor expansion | ASSIGNED |
| GPIO9 | I2C SCL — ADS1115 / sensor expansion | ASSIGNED |
| GPIO10 | MVP260 display DATA | ASSIGNED |
| GPIO11 | MVP260 display CLOCK | ASSIGNED |
| GPIO12 | MVP260 WARM button emulation | ASSIGNED |
| GPIO13 | MVP260 COOL button emulation | ASSIGNED |
| GPIO14 | MVP260 JETS button emulation | ASSIGNED |
| GPIO15 | MVP260 LIGHT button emulation | ASSIGNED |
| GPIO16 | Heater SSR MOSFET driver | ASSIGNED |

## Relay channel allocation

| Relay | GPIO | Function |
|---|---:|---|
| CH1 | GPIO1 | Pump LOW contactor |
| CH2 | GPIO2 | Pump HIGH contactor |
| CH3 | GPIO41 | Heater safety contactor |
| CH4 | GPIO42 | UV / ozone |
| CH5 | GPIO45 | Spa light |
| CH6 | GPIO46 | Spare / future |

## Currently free / candidate GPIOs

The following exposed pins are currently unassigned and may be used later after bench verification:

- GPIO3
- GPIO4
- GPIO5
- GPIO6
- GPIO7
- GPIO35
- GPIO36
- GPIO37
- GPIO39
- GPIO40
- GPIO47
- GPIO48

Do not allocate any of these permanently without first checking:
- Waveshare schematic
- boot/strap behavior
- conflicts with onboard peripherals
- voltage compatibility

## Rules

1. No user-interface code may directly control mains outputs.
2. Matter, web UI and MVP260 handlers only submit requests to the safety state machine.
3. GPIO assignments in firmware must be defined in one central config file.
4. Any pin change must be reflected here before firmware is merged.
