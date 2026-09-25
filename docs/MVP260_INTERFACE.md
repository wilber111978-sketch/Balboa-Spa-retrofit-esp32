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
