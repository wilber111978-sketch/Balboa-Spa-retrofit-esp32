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
| CS | GPIO10 |
| MOSI / DIN | GPIO11 |
| SCLK | GPIO12 |
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
- SPI lines direct to ESP32-S3 (GPIO10/11/12 are the native FSPI IOMUX pins)
- topside is ≤ 400 mm from the controller: start SPI at 20–40 MHz, reduce only if artefacts appear
- run a GND conductor alongside SCLK in the cable
- add local decoupling near the display (100 nF + optional 10 µF)
- use a transistor/MOSFET for backlight if module current requires it
- the display module is not waterproof: mount it behind a sealed clear window / gasketed bezel

### Buttons
Recommended:
- one side of each button -> GND
- other side -> assigned GPIO
- external 10 kΩ pull-up to 3.3 V and 100 nF to GND at the controller end
- INPUT_PULLUP may also be enabled (harmless in parallel)

Pressed = LOW.

## Button behavior

### Jets
Cycle:
- OFF -> LOW -> HIGH -> OFF

The safety state machine performs the actual sequencing. After each change, presses are ignored for 2 s (not queued); the pump icon briefly flashes to show the press was ignored.

### Light
- short press: light ON / OFF (2 s lockout applies)
- long press (1 s) while ON: next colour / program
- auto-off after 40 min

### Button combinations

| Combination | Hold | Action |
|---|---|---|
| Temp+ or Temp− during priming | press | end priming early |
| Temp+ and Temp− | 5 s | reset latched safety fault |
| Jets and Light | 3 s | open Setup screen (Wi-Fi status, address, app QR, Matter QR) |
| Jets (on Setup screen) | 10 s | clear Wi-Fi and restart in setup access-point mode |
| Temp− (on Setup screen) | 5 s | generate a new web app token |

See [PROVISIONING_AND_UPDATES.md](PROVISIONING_AND_UPDATES.md).

### Temp +
Increase setpoint by configured step.

### Temp -
Decrease setpoint by configured step.

Setpoint is clamped to 10–40 °C.

### Local fault reset
Hold **Temp +** and **Temp −** together for 5 s to reset a latched safety fault. This is the only way to clear latched critical faults — they cannot be cleared from the web UI or Matter.

## UI behavior

Default screen:
- current water temperature (last valid reading; show its age, e.g. "12 min ago", once older than 5 min)
  - while a reading is settling: keep the held value and show a small "reading…" icon
  - after power-up / reboot (no valid reading yet): show `--.-°`, "Checking temperature…" and a countdown (e.g. "45 s"); never show the value from before the power loss
- target temperature
- heater status
- pump/jets state

Secondary indicators:
- UV
- Wi-Fi
- Matter
- fault/warning icon

During priming:
- "Priming" with countdown; temperature shown as `--.-°` until the first valid reading

Setup screen:
- QR codes fit in the inscribed square of the round display (~170 × 170 px)

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
