# GPIO Map — Waveshare ESP32-S3 Relay 6CH

This map is based on Waveshare's published schematic / documentation for the ESP32-S3-Relay-6CH.

## Onboard functions already occupied

| GPIO | Onboard function |
|---:|---|
| GPIO1 | Relay CH1 |
| GPIO2 | Relay CH2 |
| GPIO41 | Relay CH3 |
| GPIO42 | Relay CH4 |
| GPIO45 | Relay CH5 |
| GPIO46 | Relay CH6 |
| GPIO17 | RS485 TX |
| GPIO18 | RS485 RX |
| GPIO21 | buzzer |
| GPIO38 | onboard RGB LED |
| GPIO0 | BOOT |

## Proposed external GPIO allocation

| GPIO | Proposed use | Direction |
|---:|---|---|
| GPIO8 | I2C SDA — ADS1115 / expansion | bidirectional |
| GPIO9 | I2C SCL — ADS1115 / expansion | output |
| GPIO10 | MVP260 display DATA input | input |
| GPIO11 | MVP260 display CLOCK input | input / interrupt |
| GPIO12 | MVP260 WARM emulation | output |
| GPIO13 | MVP260 COOL emulation | output |
| GPIO14 | MVP260 JETS emulation | output |
| GPIO15 | MVP260 LIGHT emulation | output |
| GPIO16 | heater SSR transistor driver | output |

These pins are exposed on the Waveshare expansion headers and do not conflict with the six onboard relay channels or onboard RS485 in the current design.

## Relay allocation

| Relay | Function |
|---|---|
| CH1 / GPIO1 | Pump LOW contactor coil |
| CH2 / GPIO2 | Pump HIGH contactor coil |
| CH3 / GPIO41 | Heater safety contactor coil |
| CH4 / GPIO42 | UV / ozone |
| CH5 / GPIO45 | Spa light |
| CH6 / GPIO46 | Spare |

## Why the SSR is not driven by CH3

The heater SSR is intended to do normal repetitive heater switching, including possible slow time-proportioning control later.

Using a mechanical onboard relay to drive the SSR would reintroduce relay wear.

Instead:

- GPIO16 drives a small transistor stage
- the transistor switches the SSR's low-current 24 VDC input
- CH3 controls the independent heater safety contactor

That separates:

- **normal heat modulation:** solid state
- **safety isolation:** mechanical contactor

## Reserved / avoid

Avoid reusing GPIOs already allocated to:
- relay channels
- RS485
- onboard RGB / buzzer
- BOOT

Do not finalize the pin map in firmware until the physical Waveshare board has been bench-tested.
