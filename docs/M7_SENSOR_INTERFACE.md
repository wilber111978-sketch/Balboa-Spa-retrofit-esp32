# OEM M7 Sensor Electrical Interface

## Confirmed Balboa M7 sensor behavior

Balboa's M7 service documentation provides a resistance/temperature table for the two heater sensors.

The published table is consistent with an approximately **30 kΩ NTC sensor at 25 °C**, not a generic 10 kΩ NTC.

Important: some aftermarket product pages describe Balboa sensors as 10 kΩ devices. The Balboa service resistance table is the reference for this project, and the actual OEM sensors should be measured before final component values are locked.

## Balboa resistance table

| Temperature °C | Resistance |
|---:|---:|
| -23.0 | 320 kΩ |
| -13.0 | 184 kΩ |
| 0.0 | 95 kΩ |
| 11.5 | 55 kΩ |
| 18.5 | 40 kΩ |
| 25.0 | 30 kΩ |
| 29.0 | 25.5 kΩ |
| 34.5 | 20.2 kΩ |
| 37.0 | 18.1 kΩ |
| 38.5 | 17.2 kΩ |
| 40.0 | 16.2 kΩ |
| 41.0 | 15.4 kΩ |
| 42.5 | 14.7 kΩ |
| 43.5 | 14.1 kΩ |
| 44.5 | 13.6 kΩ |
| 45.0 | 13.2 kΩ |
| 46.0 | 12.7 kΩ |
| 47.5 | 12.1 kΩ |
| 48.5 | 11.7 kΩ |
| 49.5 | 11.1 kΩ |
| 52.5 | 10.0 kΩ |
| 55.0 | 9.0 kΩ |
| 62.5 | 7.0 kΩ |
| 72.0 | 5.0 kΩ |
| 88.0 | 3.0 kΩ |
| 110.5 | 1.5 kΩ |

## Current firmware status

V0.6.2 uses ADS1115 AIN0/AIN1, a 3.3 V divider with 30 kΩ reference resistors, and an assumed 30 kΩ / beta 3860 thermistor model. The table above is a reference for future calibration, not the conversion currently implemented. Neither heater output is commissioned.

## Recommended readout method

Do not depend on the ESP32's raw ADC for the final heater-safety measurement if avoidable.

Recommended V1 hardware:

- ADS1115 16-bit ADC
- 3.3 V supply
- one channel per M7 sensor
- 30 kΩ reference resistor per channel, matching the reviewed firmware model
- small RC input filter
- optional series resistor / clamp protection
- common low-voltage ground with the controller sensor domain

Example divider per sensor:

```
3.3 V
  |
30 kΩ reference
  |
  +------ ADS1115 input
  |
M7 NTC sensor
  |
 GND
```

At representative spa temperatures this gives comfortably measurable voltages:

- 25 °C / 30 kΩ: ~1.65 V
- 38.5 °C / 17.2 kΩ: ~1.20 V
- 42.5 °C / 14.7 kΩ: ~1.09 V
- 52.5 °C / 10 kΩ: ~0.825 V

## Fitted front end (Nov 2026)

Each sensor has its own divider; the two channels share only a filtered sensor rail.

```
3V3 --100 Ω--+-- sensor rail (≈3.27 V) ----------------> ADS1115 A2 (when fitted)
             |
          100 µF (+ to rail, − to GND)

sensor rail --30 kΩ--+-- junction A --1 kΩ--+--> GPIO8 now / ADS1115 A0 later
                     |                      |
                 M7 sensor A             100 nF (ceramic, at the input pin)
                     |                      |
                    GND                    GND
(sensor B identical: 30 kΩ, 1 kΩ, 100 nF → GPIO9 now / ADS1115 A1 later)
```

- Parts: 2 × 30 kΩ, 2 × 1 kΩ, 2 × 100 nF ceramic ("104"), 1 × 100 Ω (Jaycar RR0548), 1 × 100 µF electrolytic. All grounds to the Waveshare header GND.
- Never share one 30 kΩ between both sensors: the sensors would be in parallel and A/B could not be told apart.
- **Now (onboard ADC):** `SPA_TEMP_ADC_ADS1115 0`, `RAIL_REF_FITTED false`. Readings run about 0.25 °C warm because the firmware assumes a 3.30 V rail; the offset is the same on both channels.
- **When the ADS1115 is fitted:** move the outputs to A0/A1, add the rail to A2, then wire VDD→3V3 (never 5 V: the board's I²C pull-ups go to VDD), GND, SCL→GPIO9, SDA→GPIO8, ADDR→GND (address 0x48), ALRT unconnected. Set `SPA_TEMP_ADC_ADS1115 1` and `RAIL_REF_FITTED true` (ratiometric correction from A2). Remove the sensor outputs from GPIO8/9 first; those pins become the I²C bus. Check the chip is marked BOGI (BRPI is an ADS1015) and that raw counts are not all multiples of 16.
- Bench checks (unpowered): rail–junction 30 kΩ each, junction A–junction B 60 kΩ, junction–output 1 kΩ, 3V3–rail 100 Ω. Powered with sensors unplugged, both outputs sit near the rail (open-sensor fault); with sensors at about 25 °C, about 1.6 V.

## Future calibration / conversion strategy

Use the Balboa published resistance table as the primary conversion reference.

Recommended firmware method:

1. read ADC voltage
2. calculate thermistor resistance from the divider equation
3. convert resistance to temperature using table interpolation
4. retain raw resistance in diagnostics

This is preferable to assuming an unknown Beta coefficient.

## Sensor plausibility

Fault immediately if:
- ADC is near rail
- calculated resistance is outside the expected physical range
- connector appears open circuit
- connector appears short circuit

The two sensors should also be cross-checked against one another.

Balboa service guidance uses matching behavior between the two sensors as part of diagnosis.

## Commissioning measurements

Before connecting the OEM sensors to the new controller:

1. unplug both sensors from the GS100
2. measure resistance of Sensor A
3. measure resistance of Sensor B
4. measure actual water / heater temperature independently
5. compare both readings against the Balboa table
6. record connector pin polarity / wire colors
7. repeat at at least two temperatures if practical

The sensors are simple resistive devices, so polarity should not matter electrically, but the harness mapping should still be documented consistently.

## Safety note

These measurements are part of the software heater-control system.

A separate independent high-limit device / safety contactor remains required so a software, ADC, wiring or sensor failure cannot leave the heater energized.
