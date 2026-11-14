# Hardware Interface — V0.9 (firmware relay map), commissioning pending

This document follows the relay allocation used by the firmware (`SpaController.h`), confirmed against the Waveshare wiki GPIO table. It is not validated installation guidance: both heater outputs remain physically inhibited in firmware until formal commissioning. Final mains work must be done or checked by a licensed electrician.

| Relay | GPIO (wiki) | Function |
|---|---:|---|
| CH1 | GPIO1 | heater safety contactor coil |
| CH2 | GPIO2 | pump LOW contactor coil |
| CH3 | GPIO41 | heater SSR control supply (dry contacts) |
| CH4 | GPIO42 | pump HIGH contactor coil |
| CH5 | GPIO45 | UV / ozone (strapping pin: may blip at boot; non-critical load) |
| CH6 | GPIO46 | spa light, 12 V AC (strapping pin: may blip at boot; non-critical load) |

The heater, pump and contactor channels (CH1–CH4) are on non-strapping GPIOs.

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
   |      +-- heater safety contactor feed (via high-limit -> LOW spare pole 1 -> CH1)
   |      +-- heater SSR control + (via high-limit -> LOW spare pole 2 -> CH3)
   |
24 VDC PSU 0 V
   +-- Waveshare 0 V
   +-- SSR control input -
   +-- low-voltage control common
```

Use a dedicated 24 V distribution terminal block.

---

## 3. Pump control schematic

### LOW

```
+24 V
  |
Waveshare CH2 COM
  |
Waveshare CH2 NO
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
Waveshare CH4 COM
  |
Waveshare CH4 NO
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

### 4.0 Hardware heater locks — LOW contactor spare poles

The LOW contactor (CJX2-K1201, 3NO main + 1NC aux) uses one main pole for the pump LOW winding and has **two spare NO main poles**. They make "heater only while LOW is pulled in" a hardware rule. Because HIGH can only close while LOW is open (NC aux interlock, §3), the heater can never run with HIGH either, whatever the firmware does.

| LOW contactor pole | Use |
|---|---|
| main pole (pump) | pump LOW winding, 240 V |
| spare pole 1 | in series with the safety contactor coil feed (§4.1), 24 V DC |
| spare pole 2 | in series with the SSR control supply through CH3 (§4.2), 24 V DC |
| NC aux 21–22 | HIGH coil interlock (§3) |

Wiring rules:
- these are mains-rated poles on a device that also carries 240 V: use mains-insulation-rated wire for the 24 V conductors, ferrule and label them "24 V DC heater lock"
- the SSR input draws only ~10 mA; power contacts may not make reliably at that current. If bench testing shows dropouts, fit a ~2.2 kΩ 1 W resistor across the SSR input to raise the contact current, or use an add-on auxiliary contact block for pole 2
- firmware always turns heat off (SSR, then contactor) before changing pump speed, so these poles never break heater current in normal use

### 4.1 Safety contactor (CH1)

```
+24 V
  |
independent high-limit, manual reset, 44 C (normally closed)
  |
  +--------------------------------------+
  |                                      |
LOW spare pole 1 (NO)              to §4.2 (SSR feed)
  |
Waveshare CH1 COM -> CH1 NO
  |
heater safety contactor coil A1 -> A2 -> 0 V   (coil suppressor fitted)
```

### 4.2 SSR control supply (CH3)

```
+24 V -> high-limit (shared, above) -> LOW spare pole 2 (NO) -> Waveshare CH3 COM -> CH3 NO -> SSR input +
SSR input - -> 0 V
```

The SSR-40DA input accepts 3–32 V DC. CH3 is a relay contact, not a direct GPIO connection.

### 4.3 High-limit status input

The firmware requires a high-limit status input before any live heating (`PIN_HIGH_LIMIT_OK`, currently `-1` = not fitted, so heating is refused). Proposed: a second, isolated contact of the high-limit (or an optocoupler across it) into **GPIO16** (free, not a strapping pin) with a 10 kΩ pull-down; HIGH = limit closed and OK. Assign it in firmware only after it has been wired and bench-tested.

### 4.4 High-limit setting — 44 °C

Simulation of a firmware fault that leaves the heater on showed water reaching ~47 °C with a typical 48 °C high-limit, and ~43 °C with 44 °C. At a 40 °C setpoint the heater outlet runs ~40.7 °C (~42 °C with a 70 % blocked filter), so 44 °C leaves margin against nuisance trips. Fit a **manual-reset high-limit, 44 °C, sensing the heater outlet/body**. Confirm no nuisance trips during wet commissioning at 40 °C.

### 4.5 Thermal fuse — last resort

If the SSR fails shorted **and** the safety contactor welds, nothing in the control circuit can stop the element (simulation: heater body >150 °C). Fit a **mains-rated one-shot thermal fuse (~70–75 °C, rated for the element current) clamped to the heater body, in series with the element**. It never operates in normal use.

### 4.6 Heater mains path

```
Active -> safety contactor pole -> SSR AC output -> thermal fuse -> 2 kW heater -> safety contactor pole 2 -> Neutral
```

The SSR requires a heatsink and thermal compound.

---

## 5. M7 temperature sensor interface

Both options use the same divider and filter per sensor:

```
3.3 V -> 30 kΩ 0.1 % reference -> node -> 1 kΩ series -> ADC input
                                    |                       |
                             Balboa M7 sensor            100 nF to GND
                                    |
                                   GND
```

The firmware converts voltage -> resistance -> temperature with the Beta model (R25 30 kΩ, Beta 3860) and per-sensor offsets for calibration. Readings only count as **water temperature** after the pump has circulated for 60 s (either speed).

### 5.1 Onboard ADC (V0.9 default, `SPA_TEMP_ADC_ADS1115 0`)

| Signal | GPIO | ADC channel |
|---|---:|---|
| Sensor A | GPIO8 | ADC1_CH7 |
| Sensor B | GPIO9 | ADC1_CH8 |
| 3.3 V reference (optional, 10k/10k divider) | GPIO3 | ADC1_CH2 |

ADC1 is used because ADC2 does not work reliably while Wi-Fi is active.

**Bench acceptance (onboard ADC):** with Wi-Fi connected, log both sensors at a steady temperature for 10 minutes. Readings must stay within ±0.1 °C, which is about 3 mV residual noise. Simulation shows the heater checks false-trip at about 6 mV. If the spread is larger, check the 1 kΩ / 100 nF filters and wiring, or fit the ADS1115. Firmware averages 32 calibrated millivolt samples per reading. Do a one-point calibration against a reference thermometer (per-sensor offset). Expect a few tenths of a degree absolute error; the A/B difference used by the heater checks is better than that, because both sensors share the ADC.

### 5.2 ADS1115 (recommended for the finished spa, `SPA_TEMP_ADC_ADS1115 1`)

| Signal | Connection |
|---|---|
| I²C SDA / SCL | GPIO8 / GPIO9 |
| Sensor A / B | A0 / A1 |
| 3.3 V reference (optional, direct) | A2 |
| 24 V supply monitor (optional, 100k/10k) | A3 |

Switching from onboard to ADS1115 is a wiring change plus that one build flag; the sensor dividers stay the same.

### 5.3 Optional references

- `RAIL_REF_FITTED`: measure the 3.3 V rail that feeds the dividers, so rail drift does not appear as temperature error.
- `SUPPLY_MON_FITTED` (ADS1115 mode): detects a 24 V sag that drops the contactors while the ESP32 keeps running. Heating is held off until 24 V has been back above 21 V for 5 s.

### 5.4 RTC (planned)

There is no RTC on the board (wiki). Planned: DS3231 on I²C. In ADS1115 mode it shares GPIO8/9. In onboard-ADC mode those pins are analog inputs, so the RTC would go on its own I²C pins (proposed GPIO47 SDA / GPIO48 SCL).

---

## 6. Replacement topside

See `TOPSIDE_V08_HARDWARE.md`: GC9A01 on GPIO10–15 and buttons on GPIO4–7, as implemented in `TopsideConfig.h`. The cable run is ≤ 400 mm. The retired VL260/MVP260 interface is archived in `archive/`.

---

## 7. UV / ozone output (CH5)

```
240 V active -> Waveshare CH5 COM / NO -> OEM UV / ozone connector
```

V0.9 rule: UV/ozone runs whenever the pump runs (LOW or HIGH), 1 s after the pump starts. It is off when the pump is stopped, during a speed changeover, and in faults. If the device inrush is significant, CH5 should drive a small contactor.

---

## 8. Light output (CH6)

A dedicated 12 V AC supply feeds the 2-wire RGB light through CH6, switching the 12 V side.

```
240 VAC -> 12 V AC supply -> CH6 COM / NO -> light (2-wire) -> supply return
```

Colour is selected by brief power interruptions. V0.9 firmware:
- each step is 500 ms off then 500 ms on
- switch-on always starts from a known colour: if the light has been off for less than 10 s, firmware waits out the reset time, then steps to the last chosen colour
- the light turns off automatically after 40 min

Set `LIGHT_PROGRAM_COUNT` and the step and reset times from a bench measurement (default 7 programs). After a power cut the light is OFF and assumed to restart at program 1.

---

## 9. Protection / layout rules

Keep physical separation between 240 V mains wiring, 24 V control wiring and sensor / display logic wiring. Use DIN terminals, a PE earth block, a neutral block, strain relief, ferrules, wire labels and suitable branch protection. Do not route M7 sensor or topside wires alongside pump/heater mains conductors.

---

## 10. Commissioning order

1. 24 V PSU only
2. Waveshare only; check relay polarity and boot behaviour (watch CH5/CH6 at boot)
3. pump contactor coils without mains load; verify the hardware LOW/HIGH interlock
4. heater locks, no mains load: with LOW off, energise CH1 and CH3. The safety contactor must not pull in and the SSR input LED must stay off. With LOW on, both work. With HIGH on, neither works. Opening the high-limit drops both.
5. sensors with dummy resistors (onboard ADC or ADS1115), then the OEM M7 sensors; one-point calibration
6. topside display and buttons
7. light: count colours and measure step and reset times
8. controlled mains commissioning by a licensed electrician; confirm no high-limit nuisance trips at 40 °C
