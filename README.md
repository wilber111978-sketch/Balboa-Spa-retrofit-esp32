# Balboa Spa Retrofit ESP32

Modern retrofit controller for a Fisher 3-person spa originally using a **Balboa GS100** power/control board and **Balboa MVP260** topside panel.

The goal is to retain the OEM spa experience and existing harnesses, while replacing the aging GS100 with an ESP32-S3 based controller that adds:

- Wi-Fi control and monitoring
- Google Home via Matter
- a local custom web interface
- OEM-style M7 heater/flow logic
- familiar short fault codes on the MVP260
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
- Balboa MVP260 topside panel
- single 2-speed pump
- 2 kW heater
- UV / ozone auxiliary
- spa light
- dual heater temperature sensors using Balboa-style M7 logic

The 2 kW heater draws about **8.3 A at 240 V**, so the replacement must preserve low-amperage behavior:

- pump LOW + heater: allowed
- pump HIGH + heater: not allowed

---

## Selected controller

### Waveshare ESP32-S3 6-channel industrial relay module

Selected because it provides:

- ESP32-S3
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
| CH3 | Heater control / safety logic |
| CH4 | UV / ozone |
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

---

## MVP260 topside

The original **Balboa MVP260** panel will be retained.

The new controller should:

- read local button presses
- maintain familiar local control
- show familiar short fault codes
- continue working when Wi-Fi or cloud access is unavailable

The open-source **kgstorm Balboa GS100 / VL260 ESP32 project** is a key reference for:

- topside signaling
- button emulation
- display decoding
- error-code handling

Although that project is named around the VL260, it is relevant to the MVP260-style Balboa interface used here.

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

The MVP260 can continue to show short OEM-style faults such as `Sn`, `dr`, etc.

The web interface should provide much richer information.

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
  MVP/VL topside decoding, display, button emulation, fault handling

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
- heater only allowed with valid sensors and safe thermal behavior
- all sensor faults force heater OFF
- controller reboot defaults to safe outputs
- Wi-Fi / Matter failure must not affect local spa operation
- ESP32 software is never the sole heater safety mechanism

---

## Project documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Bill of Materials](docs/BOM.md)
- [Fault and diagnostics specification](docs/FAULTS.md)

---

## Electrical safety

This project contains mains electricity, water, a heater and a motor.

Final mains wiring, earthing, branch protection, enclosure suitability, RCD/RCBO protection and commissioning should be inspected and tested by a suitably licensed electrician before the spa is returned to service.
