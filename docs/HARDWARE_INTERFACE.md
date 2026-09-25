# Hardware Interface Schematic — V1

This document defines the first practical wiring/schematic for the replacement controller.

It is intended to become the basis for a small interface PCB or DIN-rail daughter board.

---

## 1. System block diagram

```
                        240 VAC / 10 A SPA SUPPLY
                                  |
                     upstream RCD/RCBO already fitted
                                  |
               +------------------+------------------+
               |                  |                  |
               |                  |                  |
           Pump power         Heater power        24 V PSU
               |                  |                  |
      +--------+--------+         |                  +---- Waveshare
      |                 |         |                  +---- contactor coils
 LOW contactor      HIGH contactor|                  +---- interface electronics
      |                 |         |
      +------ 2-speed pump -------+
                                  |
                         heater safety contactor
                                  |
                              SSR-40DA
                                  |
                              2 kW heater
```

---

## 2. 24 V control distribution

Recommended:

```
24 VDC PSU +
   |
   +-- fused / protected 24 V distribution
   |      |
   |      +-- Waveshare 7–36 V input
   |      +-- LOW contactor coil feed
   |      +-- HIGH contactor coil feed
   |      +-- heater safety contactor feed
   |      +-- heater SSR input +
   |
24 VDC PSU 0 V
   +-- Waveshare 0 V
   +-- SSR driver transistor emitter/source
   +-- low-voltage control common
```

Use a dedicated 24 V distribution terminal block.

---

## 3. Pump control schematic

### LOW

```
+24 V
  |
Waveshare CH1 COM
  |
Waveshare CH1 NO
  |
HIGH contactor NC auxiliary (21-22)
  |
LOW contactor coil A1
  |
LOW coil A2
  |
0 V
```

### HIGH

```
+24 V
  |
Waveshare CH2 COM
  |
Waveshare CH2 NO
  |
LOW contactor NC auxiliary (21-22)
  |
HIGH contactor coil A1
  |
HIGH coil A2
  |
0 V
```

Fit suppression across each 24 VDC coil.

This creates a real electrical interlock independent of firmware.

---

## 4. Heater control schematic

### 4.1 Safety contactor

```
+24 V
  |
hardware high-limit / emergency permissive
  |
Waveshare CH3 COM
  |
Waveshare CH3 NO
  |
heater safety contactor coil A1
  |
A2
  |
0 V
```

If the independent high-limit opens, the safety contactor cannot energize even if the ESP32 requests heat.

### 4.2 SSR normal control

Use a dedicated transistor driver from **GPIO16**.

Suggested simple low-side driver:

```
GPIO16 ---- 2.2k ---- base 2N2222 / BC337
                       |
                    10k to GND

+24 V ---------------- SSR INPUT +
SSR INPUT - ----------- transistor collector
transistor emitter ---- 0 V
```

A small logic-level MOSFET may be used instead.

The SSR input is optically isolated internally, so the transistor stage is primarily for level shifting / current handling.

### 4.3 Heater mains path

```
Active
  |
heater safety contactor main pole
  |
SSR AC output
  |
heater element
  |
Neutral
```

A 2-pole safety contactor may be used to disconnect both conductors where appropriate.

The SSR requires a heatsink and thermal compound.

---

## 5. M7 temperature sensor interface

Use an external **ADS1115 16-bit ADC** over I2C.

Proposed pins:

- SDA: GPIO8
- SCL: GPIO9

### Sensor A divider

```
3.3 V
  |
10 kΩ, 0.1%
  |
  +------ ADS1115 A0
  |
Balboa M7 Sensor A
  |
 GND
```

### Sensor B divider

```
3.3 V
  |
10 kΩ, 0.1%
  |
  +------ ADS1115 A1
  |
Balboa M7 Sensor B
  |
 GND
```

Recommended input conditioning on each ADC channel:

- 1 kΩ series resistor between divider node and ADC input
- 100 nF capacitor from ADC input to GND
- optional clamp protection if bench measurements show transients

The final resistor/filter values remain subject to bench testing against the OEM sensors.

Firmware should convert measured voltage -> resistance -> temperature using the Balboa resistance table rather than assuming a Beta coefficient.

---

## 6. MVP260 interface

Reference project wiring indicates the panel uses an RJ45-style connector and separate lines for four buttons plus display DATA / CLOCK.

Reference pin functions:

| RJ45 pin | Function |
|---:|---|
| 1 | VIN |
| 2 | Warm |
| 3 | Light |
| 4 | GND |
| 5 | Display Data |
| 6 | Display Clock |
| 7 | Jets |
| 8 | Cool |

This must still be confirmed on the actual Fisher panel before final connection.

### Display DATA input

Reference circuit:

```
MVP260 DATA
   |
 2.2 kΩ
   |
   +---- 220 Ω ---- GPIO10
   |
 4.7 kΩ
   |
  GND
```

The open-source reference uses a 2.2 kΩ / 4.7 kΩ divider to bring the nominal 5 V display line into the ESP32 range.

### Display CLOCK input

Use the same divider / series protection:

```
MVP260 CLOCK
   |
 2.2 kΩ
   |
   +---- 220 Ω ---- GPIO11
   |
 4.7 kΩ
   |
  GND
```

GPIO11 should be configured as the timing / interrupt input.

### Button emulation

Use four optocouplers, following the proven kgstorm architecture.

ESP32 side:

- GPIO12 -> WARM optocoupler LED
- GPIO13 -> COOL optocoupler LED
- GPIO14 -> JETS optocoupler LED
- GPIO15 -> LIGHT optocoupler LED

Each LED requires an appropriate current-limiting resistor.

The optocoupler transistor side should be wired across the same two nodes the physical MVP260 button connects when pressed, matching the reference project's proven orientation.

Do not guess optocoupler polarity from wire color; verify against the panel and reference PCB before soldering.

---

## 7. UV / ozone output

For V1:

```
240 V active
   |
Waveshare CH4 COM / NO
   |
OEM UV / ozone connector
```

Neutral and protective earth continue through proper distribution blocks.

Before commissioning, verify the UV / ozone device current and startup characteristics.

If its ballast/inrush is significant, CH4 should drive a small external contactor instead of carrying the load directly.

---

## 8. Light output

CH5 is reserved for the original spa light.

Do **not** wire this section until the original light voltage has been confirmed.

Balboa installations can use low-voltage lighting, so the actual Fisher configuration must be measured / traced first.

---

## 9. Protection / layout rules

Keep physical separation between:

- 240 V mains wiring
- 24 V control wiring
- sensor / MVP260 logic wiring

Use:
- DIN terminals
- PE earth block
- neutral block
- strain relief
- ferrules
- wire labels
- suitable branch protection

Do not route M7 sensor or MVP260 data wires alongside the pump/heater mains conductors if avoidable.

---

## 10. Commissioning order

1. 24 V PSU only
2. Waveshare only
3. test all GPIO allocation
4. test pump contactor coils without mains load
5. verify hard LOW/HIGH interlock
6. test SSR driver with low-voltage indicator/load
7. connect ADS1115 and dummy resistors
8. connect OEM M7 sensors and compare resistance/temperature
9. connect MVP260 interface and verify protocol
10. only then begin controlled mains commissioning

Final mains installation and safety testing should be completed / checked by a suitably licensed electrician.
