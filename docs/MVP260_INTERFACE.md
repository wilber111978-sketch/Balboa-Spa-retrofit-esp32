# MVP260 Interface Notes

## Goal

Retain the original Balboa MVP260 topside panel as the primary local user interface while replacing the GS100 power/control board.

The interface must:

- preserve all original panel buttons
- preserve familiar temperature display and short fault codes
- work without Wi-Fi
- isolate / level-shift panel signals safely into the ESP32
- allow the ESP32 to emulate button behavior where needed
- remain electrically separate from mains switching

## Reference project

The main open-source reference is:

**kgstorm / Balboa-GS100-with-VL260-topside**

That project is valuable for:

- panel pinout concepts
- button signal handling
- display data / clock decoding
- fault-code decoding
- ESP32 integration
- optocoupler-based button emulation

Although the repository is named around VL260, the same style of Balboa duplex topside interface is relevant to the MVP260 used in this Fisher spa.

## Interface philosophy

Do not connect the ESP32 GPIO directly to unknown panel lines.

Use:
- input buffering / level shifting
- optocouplers or transistor isolation for button emulation
- ESD / transient protection where practical
- a removable panel connector / extension lead so the OEM harness is not cut

## Proposed signal classes

The exact pin assignment must be confirmed against the actual MVP260 cable and reference project before soldering.

Expected categories:

- low-voltage supply
- ground
- button lines
- display data
- display clock

## Firmware behavior

The ESP32 should maintain an internal spa state and use the MVP260 as a local front-end.

Panel actions should map to controller actions such as:

- temperature up
- temperature down
- jets cycle
- light toggle
- mode selection where applicable

The ESP32 should also be able to present short OEM-style fault information while the web UI contains the detailed fault explanation.

## Local-first rule

Loss of:
- Wi-Fi
- Google Home
- Matter
- remote access

must not prevent normal MVP260 operation.

## Commissioning plan

1. bench power the interface only
2. confirm panel supply voltage
3. confirm each button line behavior
4. confirm display data and clock levels
5. decode display frames
6. validate temperature display
7. validate fault-code rendering
8. only then connect to the main spa controller state machine


## Confirmed reference pinout from kgstorm GS100/VL260 project

The reference project reports the following RJ45-style topside wiring for the VL200/VL400-family 3/4-button panel interface:

| Pin | Function |
|---:|---|
| 1 | VIN |
| 2 | Warm button |
| 3 | Light button |
| 4 | GND |
| 5 | Display data |
| 6 | Display clock |
| 7 | Jets button |
| 8 | Cool button |

Observed electrical behavior in that project:

- button lines sit at approximately 2.5 V when idle
- a button press connects the relevant line toward 5 V
- optocouplers are used to emulate button presses without loading the panel
- display data and clock are reduced to ESP32-safe levels using resistor dividers
- display frames are 24 bits total
- display data is sampled on clock rising edges
- the project reports roughly 19 ms between frames

### Important verification rule

This pinout is an excellent starting point for the MVP260 because the GS100 technical documentation lists the MVP260 as the VL260 panel option and the kgstorm project targets the same simple Balboa panel family.

However, this project will still verify the actual Fisher MVP260 harness with a multimeter / logic analyzer before connecting it to the Waveshare controller.

Do not rely solely on wire color.

### Suggested MVP260 interface hardware

- RJ45-style mating socket / extension lead
- 4 optocoupler channels for Warm / Cool / Jets / Light button emulation
- divider / buffer inputs for Display Data and Clock
- ESD protection
- removable low-voltage connector between interface board and main controller

The display decoder can be ported from the open-source reference rather than re-created from scratch.
