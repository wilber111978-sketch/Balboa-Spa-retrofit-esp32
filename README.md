# Balboa Spa Retrofit ESP32

Modern retrofit controller for a Fisher 3-person spa originally using a **Balboa GS100** power/control board and **Balboa VL260** topside panel.

The goal is to retain the OEM spa experience and existing harnesses, while replacing the aging GS100 with an ESP32-S3 based controller that adds:

- Wi-Fi control and monitoring
- Google Home via Matter
- a local custom web interface
- OEM-style M7 heater/flow logic
- a new local 1.28-inch round TFT topside with descriptive status and faults
- detailed fault history and probable-cause diagnostics
- solar-priority heating
- proper motor contactors and SSR heater switching
- future pH / ORP monitoring and dosing support

> **Status:** hardware design and parts selection in progress. Major control hardware has been ordered. Enclosure sizing will be done after bench layout.

---

## Existing spa hardware

- Fisher 3-person spa
- 230–240 V AC
- standard 10 A plug supply
- upstream RCD/RCBO protection
- Balboa GS100 controller
- original Balboa VL260 topside panel (listed as MVP260 in GS100 documentation; being retired — this project calls it "VL260" throughout)
- replacement topside: 1.28-inch round GC9A01 240×240 TFT + 4 waterproof momentary buttons
- single 2-speed pump
- 2 kW heater
- UV / ozone auxiliary
- spa light
- dual heater temperature sensors using Balboa-style M7 logic

The 2 kW heater draws about **8.3 A at 240 V**, so the replacement must preserve low-amperage behavior:

- pump LOW + heater: allowed
- pump HIGH + heater: not allowed

This is the same load pattern the original GS100 ran for years on the same 10 A outlet.

---

## Operating rules (summary)

| Rule | Enforced by |
|---|---|
| LOW and HIGH never energised together | contactor NC aux interlock + firmware |
| Heater only when pump LOW contactor is pulled in | LOW contactor spare NO poles (hardware) + firmware |
| Heater never with pump HIGH | follows from the two rules above + firmware |
| Heater off on over-temperature | independent manual-reset high-limit (hardware) + firmware |
| UV / ozone runs whenever the pump runs (LOW or HIGH), off when the pump is stopped | firmware |
| Water temperature sampled continuously; pump LOW polled on a timer when the pump is idle so the reading stays current | firmware |
| Setpoint hard-limited to 10–40 °C | firmware |
| Daily filter cycles run independently of heat demand | firmware |

---

## Selected controller

### Waveshare ESP32-S3-Relay-6CH

Selected because it provides:

- ESP32-S3 (ESP32-S3-WROOM-1U-N16 module: 16 MB flash, no PSRAM — confirm from the module label)
- Wi-Fi / Bluetooth
- 6 relay outputs
- RS485
- external antenna
- 7–36 VDC supply input
- industrial-style screw terminals / enclosure

### Planned output allocation

| Channel | Function |
|---|---|
| CH1 | Pump LOW contactor |
| CH2 | Pump HIGH contactor |
| CH3 | Heater safety contactor coil |
| CH4 | UV / ozone (runs with pump LOW or HIGH) |
| CH5 | Spa light |
| CH6 | Spare / future expansion |

---

## Control power

A **24 VDC DIN-rail PSU, 60 W / 2.5 A** will power:

- Waveshare controller
- pump contactor coils
- heater safety contactor coil
- control-side sensors / auxiliaries

The Waveshare accepts 24 VDC directly.

If a future interface requires 5 V, a small 24 V -> 5 V DC-DC converter can be added.

---

## Pump control

Two motor contactors are used rather than switching the pump through PCB relays.

### Selected contactors

**2 x CJX2-K1201 DC**

- 12 A AC-3
- 24 VDC coil
- 3NO main contacts
- 1NC auxiliary contact

One controls LOW speed and one controls HIGH speed.

### Hardware interlock

Each contactor's NC auxiliary contact is placed in series with the opposite contactor coil.

This means both pump windings cannot be energized simultaneously even if firmware fails.

Firmware will also enforce a changeover dead-time of approximately 0.5–1.0 s.

---

## Heater control

### Heater

- 2 kW
- approximately 8.3 A at 240 V

### Normal switching

A **zero-cross DC-to-AC SSR** is used for normal heater cycling.

Current candidate:

- JOTTA SSR-40DA
- 40 A nominal
- DC input
- AC output
- zero-cross

The SSR requires:

- proper heatsink
- thermal compound
- sensible thermal clearance
- optional heatsink temperature monitoring

### Independent safety contactor

The SSR is **not** the only heater disconnect device.

A separate 24 VDC-coil contactor will be installed in series with the heater.

Power path:

```
240 VAC
  |
heater safety contactor
  |
zero-cross SSR
  |
2 kW heater
```

### Hardware heater permissive from the LOW pump contactor

The LOW pump contactor has two spare NO main poles. They are used as hardware heater permissives:

- **Spare pole 1** is in series with the heater safety contactor coil feed.
- **Spare pole 2** is in series with the heater SSR's 24 VDC input feed.

So the heater cannot be energised unless the LOW contactor is physically pulled in, regardless of what the firmware does. Because the HIGH contactor can only close when LOW is open (NC interlock), this also makes "no heater on HIGH" a hardware rule.

See [docs/HARDWARE_INTERFACE.md](docs/HARDWARE_INTERFACE.md) §4.

The safety contactor drops out for:

- sensor failure
- over-temperature
- low/no-flow condition
- controller fault
- service / emergency isolation

---

## M7-style heater and flow logic

The retrofit will preserve the OEM Balboa philosophy using the two temperature sensors fitted either side of the heater.

The ESP32 will monitor:

- Sensor A
- Sensor B
- absolute temperature
- heater delta-T
- rate of change
- pump state
- heater state
- fault retry / lockout timing

### Normal sequence

1. start pump LOW
2. allow temperatures to stabilize
3. validate Sensor A and Sensor B
4. compare sensor agreement
5. enable heater safety contactor
6. command heater SSR
7. continuously monitor delta-T and rise rate

### Fault examples

- sensor open / short / implausible
- sensors out of agreement
- heater outlet rises too quickly
- excessive delta-T
- heater commanded but no thermal response
- absolute heater over-temperature
- pump HIGH requested while heater active

An independent hardware high-limit remains part of the heater safety chain.

### Temperature sampling and polling

The M7 sensors sit either side of the heater, so they read true tub temperature whenever water is moving through the heater — at **either** pump speed.

- Both sensors are sampled continuously (~1 s) in every state, including pump HIGH with the heater locked out.
- A reading becomes the displayed water temperature once the pump (LOW or HIGH) has run for a settle time (~60 s).
- When the pump is stopped, the controller runs pump LOW for a short poll (~2 min) on a timer (~every 30 min) so the displayed temperature and heat demand stay current.
- The topside and web UI always show the last valid temperature and its age.

See [docs/HEATER_M7_STATE_MACHINE.md](docs/HEATER_M7_STATE_MACHINE.md).

---

## Replacement topside

The original Balboa VL260 is being removed from the replacement-controller architecture.

The new local topside is:

- 1.28-inch round IPS TFT
- GC9A01 controller
- 240 × 240 resolution
- 4-wire SPI
- 3.3 V logic
- four waterproof momentary buttons:
  - Jets
  - Light
  - Temperature +
  - Temperature -

The topside sits no more than ~400 mm from the controller and connects directly to the Waveshare ESP32-S3 header over SPI. No VL260 display clock decoding, button optocouplers or RJ45 topside protocol layer are required.

The kgstorm GS100/VL260 project remains useful only as a historical/OEM behavior reference.

---

## Plug-and-play spa harnesses

The original Fisher wiring loom will remain intact.

The new controller uses matching free-hanging female Molex housings so the existing male harness connectors plug straight in.

Confirmed connector count from the GS100:

- 2 x 4-pin
- 1 x 3-pin
- 1 x 2-pin

### Molex parts

| Qty | Part | Description |
|---:|---|---|
| 2 | 50-84-2042 | 4-position female free-hanging receptacle |
| 1 | 50-84-2032 | 3-position female free-hanging receptacle |
| 1 | 50-84-2022 | 2-position female free-hanging receptacle |
| 20 | 02-08-1002 | female crimp contacts, 14–20 AWG |

Exact pin assignments will be confirmed against the original GS100 board markings and loom before mains commissioning.

---

## Diagnostics philosophy

The TFT topside should show a concise local fault title/code, while the web interface provides richer diagnostic detail.

Each stored fault event should contain:

- timestamp
- panel fault code
- internal trigger ID
- plain-English description
- Sensor A
- Sensor B
- delta-T
- setpoint
- pump state
- heater state
- UV state
- retry count
- fault duration
- probable causes
- recommended checks

Example:

> **Sensor 2 out of range**  
> Sensor 1: 36.8 °C  
> Sensor 2: invalid / open circuit  
> Pump: LOW  
> Heater: OFF  
> Probable causes: failed sensor, unplugged connector, damaged cable, corrosion, input circuit fault.

See [docs/FAULTS.md](docs/FAULTS.md).

---

## Web interface

The ESP32 will host its own local web UI.

Main page should include:

- current water temperature
- target temperature
- heating state
- pump state
- heater state
- UV state
- M7 / flow status
- active fault
- Sensor A
- Sensor B
- heater delta-T
- jets OFF / LOW / HIGH
- light
- heating mode
- solar-priority schedule
- history
- diagnostics
- settings

The spa must remain fully functional without the web interface.

---

## Google Home / Matter

V1 remote access will use **Matter + Google Home**.

No Raspberry Pi, Home Assistant, or separate local server is required.

Suggested Matter exposure:

- thermostat-style endpoint for current and target water temperature
- jets control
- light control
- status information where practical

Google Home must never directly energize the heater SSR or motor contactors.

Remote commands request a state; the local ESP32 safety state machine decides whether the request can be executed.

---

## Solar-priority heating

V1 will support scheduled daytime heating to take advantage of solar generation.

Example:

```
09:00–15:30
```

Future versions can optionally integrate actual inverter / surplus-solar data.

Outside the solar window the controller does not heat unless the water falls below a configurable minimum (economy floor), so the spa never goes stone cold.

### Timekeeping

The Waveshare board has no RTC. A DS3231 RTC module on the existing I²C bus (GPIO8/9) keeps time through power cuts and Wi-Fi outages. NTP is used to correct it whenever Wi-Fi is available. If neither source has valid time, schedules are suspended and the spa runs in NORMAL heating mode with a warning shown.

### Filter cycles

Pump LOW runs on configurable daily filter cycles (default two cycles per day) independent of heat demand, for filtration and UV / ozone sanitation. Placing a filter cycle inside the solar window lets heating and filtration share the same pump run time.

---

## Future water chemistry

The architecture should allow later addition of:

- pH
- ORP
- temperature
- conductivity / TDS
- acid dosing
- sanitizer dosing

A PoolMaster-style isolated pH/ORP front end is being considered as a lower-cost alternative to Atlas Scientific.

For a small spa, dosing should be conservative:

- circulation required
- small measured dose
- long mixing delay
- re-measure before re-dosing
- maximum single / hourly / daily dose
- no dosing on sensor fault
- no dosing without confirmed circulation

---

## Open-source references

The design draws ideas from several existing projects rather than starting from zero:

- **kgstorm / Balboa-GS100-with-VL260-topside**  
  retained as an OEM-behavior/reference source only; the new controller no longer depends on the VL260 protocol

- **ESP32 Spa Manager**  
  replacement-controller state machine and sequencing ideas

- **ESP32 PoolMaster**  
  pH/ORP monitoring, calibration, dosing and chemistry architecture

---

## Safety rules

Minimum rules:

- LOW and HIGH pump contactors can never be active together
- hardware and software pump interlocks
- heater OFF before switching to pump HIGH
- heater only possible with pump LOW contactor pulled in (hardware permissive via LOW spare poles)
- heater only allowed with valid sensors and safe thermal behavior
- setpoint hard-limited to 40 °C maximum
- UV / ozone only while the pump is running
- latched safety faults can only be reset at the spa (local topside), not remotely
- control commands from the web API require authentication
- all sensor faults force heater OFF
- controller reboot defaults to safe outputs
- Wi-Fi / Matter failure must not affect local spa operation
- ESP32 software is never the sole heater safety mechanism

---

## Project documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Hardware interface schematic](docs/HARDWARE_INTERFACE.md)
- [Master GPIO map](docs/MASTER_GPIO_MAP.md) — the single source of truth for pin use
- [Replacement topside (GC9A01 + buttons)](docs/TOPSIDE_GC9A01.md)
- [M7 sensor interface](docs/M7_SENSOR_INTERFACE.md)
- [Heater / M7 state machine](docs/HEATER_M7_STATE_MACHINE.md)
- [Fault and diagnostics specification](docs/FAULTS.md)
- [Pin mapping and harness plan](docs/PIN_MAPPING.md)
- [Bill of Materials](docs/BOM.md)
- [Hardware inventory](docs/HARDWARE_INVENTORY.md)
- [Firmware structure](docs/FIRMWARE_STRUCTURE.md)
- [Web API contract](docs/API_CONTRACT.md)
- [Web app UI spec](docs/APP_UI_SPEC.md)
- [Web app work handoff](docs/WORK_APP_HANDOFF.md)
- [Archive: legacy VL260 topside interface](docs/archive/LEGACY_VL260_TOPSIDE_INTERFACE.md) — retired, reference only

---

## Electrical safety

This project contains mains electricity, water, a heater and a motor.

Final mains wiring, earthing, branch protection, enclosure suitability, RCD/RCBO protection and commissioning should be inspected and tested by a suitably licensed electrician before the spa is returned to service.
