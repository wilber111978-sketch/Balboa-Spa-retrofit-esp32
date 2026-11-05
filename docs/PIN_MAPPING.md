# Pin Mapping and Harness Plan

This file tracks all electrical interfaces that must be verified before final wiring.

## Existing GS100 harnesses

Confirmed from the OEM board / photo:

- 2 x 4-pin Molex-style spa connectors
- 1 x 3-pin Molex-style spa connector
- 1 x 2-pin Molex-style spa connector
- MVP260 modular topside connector

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

Do not assume the light is 240 V.

## Reviewed V0.6.2 controller mapping

| Function | Output |
|---|---|
| Heater safety contactor | CH1 / GPIO1, inhibited |
| Pump LOW | CH2 / GPIO2 |
| Heater SSR control | CH3 / GPIO41 dry relay contacts, inhibited |
| Pump HIGH | CH4 / GPIO42 |
| UV / ozone | CH5 / GPIO45 |
| Spa light | CH6 / GPIO46 |

See [MASTER_GPIO_MAP.md](MASTER_GPIO_MAP.md). GPIO16 is not used for SSR drive in this revision. Harness terminal positions and light voltage are still unverified. This records software assignments, not permission to wire or energize them.

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

## Verification checklist before mains

- identify every OEM harness by function
- continuity-test each pin to its destination
- record conductor colors
- verify earth continuity
- verify heater polarity / neutral routing
- verify UV / ozone connector assignment
- verify light voltage and connector assignment
- label each pigtail
