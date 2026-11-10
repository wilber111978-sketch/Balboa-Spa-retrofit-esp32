# Replacement Topside — GC9A01 + 4 Waterproof Buttons

## Confirmed hardware

- 1.28-inch round IPS TFT
- GC9A01 controller
- 240 × 240 pixels
- 4-wire SPI
- 2.8–3.3 V / 3.3 V logic class module
- four waterproof momentary buttons

## Button functions

1. Jets
2. Light
3. Temperature +
4. Temperature -

## GPIO allocation

### TFT

| Function | GPIO |
|---|---:|
| SCLK | GPIO10 |
| MOSI / DIN | GPIO11 |
| CS | GPIO12 |
| DC | GPIO13 |
| RESET | GPIO14 |
| Backlight | GPIO15 |

### Buttons

| Function | GPIO |
|---|---:|
| Jets | GPIO4 |
| Light | GPIO5 |
| Temp + | GPIO6 |
| Temp - | GPIO7 |

## Wiring philosophy

### TFT
- VCC -> 3.3 V
- GND -> GND
- SPI lines direct to ESP32-S3
- add local decoupling near the display
- use a transistor/MOSFET for backlight if module current requires it

### Buttons
Recommended:
- one side of each button -> GND
- other side -> assigned GPIO
- configure INPUT_PULLUP

Pressed = LOW.

## Button behavior

### Jets
Cycle:
- OFF -> LOW -> HIGH -> OFF

The safety state machine performs the actual sequencing.

### Light
Toggle light request.

### Temp +
Increase setpoint by configured step.

### Temp -
Decrease setpoint by configured step.

## UI behavior

Default screen:
- current water temperature
- target temperature
- heater status
- pump/jets state

Secondary indicators:
- UV
- Wi-Fi
- Matter
- fault/warning icon

On fault:
- replace normal secondary content with clear fault title
- allow button navigation to a short diagnostic summary if desired

## Firmware

Suggested graphics library:
- Adafruit_GC9A01A or equivalent proven GC9A01 library

Keep rendering non-blocking.

Only redraw changed regions where practical to reduce flicker and CPU use.

## Removed legacy requirements

The production replacement controller no longer requires:
- VL260 RJ45 interface
- display DATA/CLOCK decoding
- kgstorm 24-bit frame decoder
- four button-emulation optocouplers
- resistor dividers for VL260 display signals

The kgstorm project remains useful as a reference for original GS100/VL260 behavior only.
