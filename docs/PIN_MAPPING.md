# Pin Mapping and Harness Plan

This file tracks all electrical interfaces that must be verified before final wiring.

## Existing GS100 harnesses

Confirmed from the OEM board / photo:

- 2 x 4-pin Molex-style spa connectors
- 1 x 3-pin Molex-style spa connector
- 1 x 2-pin Molex-style spa connector
- VL260 (MVP260) modular topside connector — not reused; the GC9A01 replacement topside has its own cable

## Replacement cable-side housings

| Qty | Molex Part | Description |
|---:|---|---|
| 2 | 50-84-2042 | 4-position female free-hanging receptacle |
| 1 | 50-84-2032 | 3-position female free-hanging receptacle |
| 1 | 50-84-2022 | 2-position female free-hanging receptacle |
| 20 | 02-08-1002 | female crimp contacts, 14–20 AWG |

## Functional assignment

The exact position numbering and conductor colors must be confirmed from the OEM loom before energizing.

### 2-speed pump
Expected functions:
- LOW active
- HIGH active
- neutral/common
- earth

### Heater
Expected functions:
- active
- neutral
- earth

### UV / ozone
Expected functions:
- switched active
- neutral
- earth where present / applicable

### Light
Expected functions depend on the actual Fisher installation and voltage.

Do not assume the light is 240 V. It is a 2-wire RGB LED light on **12 V AC**, fed from a dedicated 12 V AC supply switched by CH5 (see [HARDWARE_INTERFACE.md](HARDWARE_INTERFACE.md) §8).

## New controller mapping

### Pump LOW
Waveshare CH1 -> 24 VDC LOW contactor coil

### Pump HIGH
Waveshare CH2 -> 24 VDC HIGH contactor coil

### Heater
- Waveshare CH3 -> heater safety contactor coil (fed via high-limit and LOW spare pole 1)
- GPIO16 -> low-side driver -> SSR input (SSR +24 V fed via LOW spare pole 2)

### UV / ozone
Waveshare CH4 (firmware runs it only while pump LOW or HIGH runs)

### Light
Waveshare CH5

### Spare
Waveshare CH6

## Interlock wiring

LOW contactor coil path:
- command
- HIGH contactor NC auxiliary
- LOW contactor coil

HIGH contactor coil path:
- command
- LOW contactor NC auxiliary
- HIGH contactor coil

This hard interlock is mandatory even though firmware also blocks simultaneous commands.

## LOW contactor pole allocation

Record the actual terminal numbers once wired.

| LOW contactor pole | Terminals (fill in) | Use | Voltage |
|---|---|---|---|
| main pole (pump) | e.g. 1/L1 – 2/T1 | pump LOW winding active | 240 VAC |
| spare pole 1 | e.g. 3/L2 – 4/T2 | heater safety contactor coil permissive | 24 VDC |
| spare pole 2 | e.g. 5/L3 – 6/T3 | heater SSR input permissive | 24 VDC |
| NC aux | 21 – 22 | HIGH coil interlock | 24 VDC |

24 V conductors on this contactor must be mains-insulation-rated and labelled, because the adjacent pole carries 240 V.

## Verification checklist before mains

- identify every OEM harness by function
- continuity-test each pin to its destination
- record conductor colors
- verify earth continuity
- verify heater polarity / neutral routing
- verify UV / ozone connector assignment
- verify light connector assignment (12 V AC from dedicated supply via CH5)
- verify heater permissive: no safety contactor / SSR operation unless LOW is pulled in
- label each pigtail
