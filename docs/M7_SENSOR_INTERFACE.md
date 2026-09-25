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

## Recommended readout method

Do not depend on the ESP32's raw ADC for the final heater-safety measurement if avoidable.

Recommended V1 hardware:

- ADS1115 16-bit ADC
- 3.3 V supply
- one channel per M7 sensor
- precision 10 kΩ 0.1% reference resistor per channel
- small RC input filter
- optional series resistor / clamp protection
- common low-voltage ground with the controller sensor domain

Example divider per sensor:

```
3.3 V
  |
10 kΩ 0.1%
  |
  +------ ADS1115 input
  |
M7 NTC sensor
  |
 GND
```

At representative spa temperatures this gives comfortably measurable voltages:

- 25 °C / 30 kΩ: ~2.48 V
- 38.5 °C / 17.2 kΩ: ~2.09 V
- 42.5 °C / 14.7 kΩ: ~1.96 V
- 52.5 °C / 10 kΩ: ~1.65 V

## Conversion strategy

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
