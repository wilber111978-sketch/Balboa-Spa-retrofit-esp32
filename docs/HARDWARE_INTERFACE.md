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
   |      +-- heater safety contactor feed (via high-limit -> LOW spare pole 1 -> CH3)
   |      +-- heater SSR input + (via LOW spare pole 2)
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

### 4.0 Hardware heater permissive — LOW contactor spare poles

The LOW pump contactor (CJX2-K1201, 3NO main + 1NC aux) uses one main pole for the pump LOW winding, leaving **two spare NO main poles**. Both are used as hardware heater permissives:

| LOW contactor pole | Use |
|---|---|
| main pole used by pump LOW winding | pump LOW active |
| **spare pole 1** | in series with heater safety contactor coil feed (§4.1) |
| **spare pole 2** | in series with heater SSR +24 V input feed (§4.2) |
| NC aux (21-22) | HIGH coil interlock (§3) |

Confirm actual pole numbering on the contactor and record it in [PIN_MAPPING.md](PIN_MAPPING.md).

Result:
- the heater cannot be energised unless LOW is physically pulled in
- HIGH can only close when LOW is open (NC interlock), so heater + HIGH is impossible in hardware
- two independent removal paths: the mains path (safety contactor) and the switching path (SSR input)

Wiring rules for these poles:
- these are mains-rated main contacts on a device that also carries 240 V; the 24 V conductors landed on them must be **mains-insulation-rated wire**, ferruled, and labelled "24 VDC — heater permissive"
- keep 24 V and 240 V poles clearly identified; do not share a pole between voltages
- main power contacts are not designed for very small signal currents; the SSR input draws only ~10 mA. If spare pole 2 proves unreliable at that current during bench testing, add a ~2.2 kΩ / 1 W bleed resistor across the SSR input to raise the contact current to ~20 mA, or move pole 2 to an add-on auxiliary contact block

Firmware sequencing always removes heat (SSR, then safety contactor) **before** dropping LOW, so these poles never break heater load in normal operation. If LOW drops unexpectedly, the SSR turns off at the next zero crossing and the safety contactor drops out.

### 4.1 Safety contactor

```
+24 V
  |
hardware high-limit (manual reset) / emergency permissive
  |
LOW contactor spare pole 1 (NO)
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

If the independent high-limit opens, or LOW is not pulled in, the safety contactor cannot energize even if the ESP32 requests heat.

Fit coil suppression across A1/A2.

### 4.2 SSR normal control

Use a dedicated low-side driver from **GPIO16** (the inventory MOSFET module, or a transistor stage).

Suggested simple low-side driver:

```
GPIO16 ---- 2.2k ---- base 2N2222 / BC337
                       |
                    10k to GND   (holds SSR OFF during ESP32 boot/reset)

+24 V ---- LOW contactor spare pole 2 (NO) ---- SSR INPUT +
SSR INPUT - ----------- transistor collector
transistor emitter ---- 0 V
```

A small logic-level MOSFET may be used instead (gate pull-down 10–100 kΩ).

The SSR input is optically isolated internally, so the driver stage is primarily for level shifting / current handling. The SSR-40DA input range is 3–32 VDC.

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

### 3.3 V reference channel (A2)

The ADS1115 measures against its own internal reference, not the 3.3 V rail that feeds the dividers. Any drift or noise on the 3.3 V rail would appear as temperature error.

```
3.3 V (same rail feeding the dividers)
  |
1 kΩ series
  |
  +------ ADS1115 A2
  |
100 nF to GND
```

Use PGA ±4.096 V on all channels. Firmware computes each sensor's resistance ratiometrically from A0/A1 against A2:

```
R_ntc = R_ref × V_sensor / (V_rail − V_sensor)
```

A3 remains spare (candidate: SSR heatsink temperature).

### RTC on the same bus

The Waveshare board has no RTC. Fit a **DS3231** module on the same I²C bus:

| Device | I²C address |
|---|---|
| ADS1115 (ADDR → GND) | 0x48 |
| DS3231 | 0x68 |

If the DS3231 module carries an onboard 24Cxx EEPROM, note its address (usually 0x57) so it is not reused. Pull-ups: use the modules' onboard pull-ups only once on the bus (remove extras so the combined value stays ≥ ~2.2 kΩ).

Firmware should convert measured voltage -> resistance -> temperature using the Balboa resistance table rather than assuming a Beta coefficient.

---

## 6. Replacement topside — GC9A01 TFT + four buttons

The original VL260 protocol interface is retired for the replacement controller.

### TFT

Hardware:
- 1.28-inch round IPS display
- GC9A01 controller
- 240 × 240
- SPI
- 3.3 V logic

Proposed wiring:

| TFT signal | Waveshare |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| CS | GPIO10 |
| DIN / MOSI | GPIO11 |
| SCLK | GPIO12 |
| DC | GPIO13 |
| RESET | GPIO14 |
| BL | GPIO15 |

GPIO10/11/12 are the ESP32-S3's native FSPI IOMUX pins (CS0 / D / CLK), so the display can run at full SPI speed without going through the GPIO matrix.

No MISO connection is required for the normal display path.

Cable run: topside is ≤ 400 mm from the controller. At that length SPI is fine; start at 20–40 MHz and only reduce if artefacts appear. Run GND alongside SCLK in the cable (ribbon: alternate signal/GND where possible).

The backlight may be driven directly only if the display module's BL input current is within the GPIO/module specification. Otherwise use a small transistor/MOSFET driver.

Add local display decoupling:
- 100 nF ceramic near the display supply
- optional 10 µF bulk capacitor near the topside connector

### Buttons

Four waterproof momentary buttons:

| Button | GPIO |
|---|---:|
| Jets | GPIO4 |
| Light | GPIO5 |
| Temperature + | GPIO6 |
| Temperature - | GPIO7 |

Recommended input method:
- one side of each button to GND
- other side to GPIO
- external 10 kΩ pull-up to 3.3 V at the controller end (internal pull-up is ~45 kΩ — acceptable at 400 mm, but the external one adds margin in a damp enclosure)
- 100 nF from GPIO to GND at the controller end (hardware debounce / noise filter)
- optional 1 kΩ series resistor between cable and GPIO for ESD protection
- software debounce
- long-press support where useful
- stuck-button timeout / fault logging

The buttons submit high-level requests to the spa state machine. They never directly energize relays/contactors.

### Topside display content

The TFT should show at minimum:
- current water temperature
- target temperature
- heating state
- pump/jets state
- light state
- fault status
- Wi-Fi / Matter indicator where practical

Faults should be more descriptive than the original VL260 2-character display.

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

If its ballast/inrush is significant, CH4 should drive a small external contactor instead of carrying the load directly. (Waveshare relays are rated ≤ 10 A at 250 VAC.)

### Run rule

UV / ozone runs whenever the pump runs, at **either** speed, and is off when the pump is stopped:

- ON after pump LOW or HIGH has been running for a short settle delay (~10 s)
- OFF during LOW↔HIGH changeover dead-time
- OFF when the pump is stopped
- runs during filter cycles, heating, temperature polls and jets HIGH

This is enforced in firmware.

---

## 8. Light output

CH5 is reserved for the original spa light: a **2-wire RGB LED light**.

### Supply — confirmed

A dedicated **12 V AC power supply** feeds the light through Waveshare relay **CH5** (switching the 12 V AC side, primary always powered). This is already in place on the bench.

```
240 VAC ---- 12 V AC supply ---- CH5 COM / NO ---- light (2-wire)
                     |------------------------------- light return
```

Switching the 12 V AC side gives clean, immediate power interruptions for colour stepping (no transformer start-up delay). 12 V AC at LED-light current is well within the CH5 relay rating.

### Colour / program control

A 2-wire light has no data line. Colour or program is selected by **briefly interrupting power**: each quick OFF→ON (typically < 1–2 s off) steps to the next colour/program; staying OFF longer resets it to the first program at next switch-on.

Firmware drives this through CH5:
- step pulse: OFF for `LIGHT_STEP_OFF_MS`, then ON for `LIGHT_STEP_ON_MS`
- to select program N from a known program, send the required number of step pulses
- firmware tracks the current program; after the light has been OFF longer than `LIGHT_RESET_OFF_S` it assumes program 1 at next switch-on
- the light is OFF at boot, so the tracked program is always in sync after power-up

Measure on the bench before setting defaults:

| Parameter | Placeholder | Measure |
|---|---|---|
| `LIGHT_PROGRAM_COUNT` | 7 | count programs before it wraps |
| `LIGHT_STEP_OFF_MS` | 500 ms | shortest off time that reliably steps |
| `LIGHT_STEP_ON_MS` | 500 ms | on time needed before the next step |
| `LIGHT_RESET_OFF_S` | 10 s | off time after which it restarts at program 1 |

Relay wear: occasional colour changes are well within the CH5 relay's life. Firmware limits step sequences to one at a time (2 s lockout after each completed sequence).

---

## 9. Protection / layout rules

Keep physical separation between:

- 240 V mains wiring
- 24 V control wiring
- sensor / TFT/button low-voltage wiring

Use:
- DIN terminals
- PE earth block
- neutral block
- strain relief
- ferrules
- wire labels
- suitable branch protection

Do not route M7 sensor or TFT/button wiring alongside the pump/heater mains conductors if avoidable.

---

## 10. Commissioning order

1. 24 V PSU only
2. Waveshare only
3. test all GPIO allocation
4. test pump contactor coils without mains load
5. verify hard LOW/HIGH interlock
6. test SSR driver with low-voltage indicator/load
7. verify hardware heater permissive (no mains load):
   - with LOW de-energised, command CH3 and GPIO16 ON: safety contactor must **not** pull in and SSR input LED must stay **off**
   - with LOW energised, both must operate
   - with HIGH energised, neither may operate
   - open the high-limit: safety contactor must drop out
8. connect ADS1115 (with A2 rail reference) and dummy resistors; verify ratiometric readings
9. connect DS3231; verify time survives power-off
10. connect OEM M7 sensors and compare resistance/temperature
11. connect GC9A01 TFT and four buttons; verify local topside UI
12. only then begin controlled mains commissioning; during first wet heat run, confirm which sensor is heater inlet (A) and outlet (B) — the outlet rises first

Final mains installation and safety testing should be completed / checked by a suitably licensed electrician.
