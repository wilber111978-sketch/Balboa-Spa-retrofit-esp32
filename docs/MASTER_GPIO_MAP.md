# Master GPIO / I/O Allocation

This is the project-wide **single source of truth** for ESP32-S3 pin usage. Update this file whenever a GPIO is reassigned. (The older `GPIO_MAP.md` has been removed because it conflicted with this file.)

## Board

**Waveshare ESP32-S3-Relay-6CH**, fitted with an **ESP32-S3-WROOM-1U-N16** module (16 MB flash, no PSRAM — confirm from the module can label). Relays are optocoupler-isolated, rated ≤ 10 A 250 VAC / 30 VDC. No onboard RTC.

All project GPIOs below are on the board's 40-pin Pico-style header.

## Onboard fixed functions

| GPIO | Function | Status |
|---:|---|---|
| GPIO1 | Relay CH1 | USED |
| GPIO2 | Relay CH2 | USED |
| GPIO41 | Relay CH3 | USED |
| GPIO42 | Relay CH4 | USED |
| GPIO45 | Relay CH5 | USED (strapping pin — may glitch at boot; keep on non-critical load) |
| GPIO46 | Relay CH6 | USED (strapping pin — may glitch at boot; keep on non-critical load) |
| GPIO17 | RS485 TX | RESERVED |
| GPIO18 | RS485 RX | RESERVED |
| GPIO21 | Buzzer | RESERVED |
| GPIO38 | RGB LED | RESERVED |
| GPIO0 | BOOT | RESERVED |

## Project assignments

### I²C bus

| GPIO | Project use | Status |
|---:|---|---|
| GPIO8 | I2C SDA — ADS1115 (0x48) + DS3231 RTC (0x68) | ASSIGNED |
| GPIO9 | I2C SCL — ADS1115 (0x48) + DS3231 RTC (0x68) | ASSIGNED |

ADS1115 channels:

| Channel | Use |
|---|---|
| A0 | M7 Sensor A (heater inlet) |
| A1 | M7 Sensor B (heater outlet) |
| A2 | 3.3 V rail reference (ratiometric correction) |
| A3 | spare (candidate: SSR heatsink temperature) |

### Replacement topside — GC9A01 1.28-inch 240×240 TFT (≤ 400 mm cable)

| GPIO | TFT function | Status |
|---:|---|---|
| GPIO10 | TFT CS (FSPI CS0 IOMUX) | ASSIGNED |
| GPIO11 | SPI MOSI / DIN (FSPI D IOMUX) | ASSIGNED |
| GPIO12 | SPI SCLK (FSPI CLK IOMUX) | ASSIGNED |
| GPIO13 | TFT DC | ASSIGNED |
| GPIO14 | TFT RESET | ASSIGNED |
| GPIO15 | TFT backlight control | ASSIGNED |

CS/MOSI/SCLK use the native FSPI IOMUX pins so the display can run at full SPI clock. (Earlier drafts had SCLK on GPIO10 and CS on GPIO12; swapped.)

TFT supply:
- VCC: 3.3 V
- GND: GND

### Waterproof momentary buttons

| GPIO | Button | Status |
|---:|---|---|
| GPIO4 | Jets | ASSIGNED |
| GPIO5 | Light | ASSIGNED |
| GPIO6 | Temperature + | ASSIGNED |
| GPIO7 | Temperature - | ASSIGNED |

Wired button-to-GND, active LOW, external 10 kΩ pull-up + 100 nF at the controller end, plus software debounce and stuck-button detection.

### Heater SSR

| GPIO | Function | Status |
|---:|---|---|
| GPIO16 | heater SSR low-side driver (10 kΩ pull-down holds OFF at boot) | ASSIGNED |

SSR +24 V feed passes through LOW contactor spare pole 2 (hardware permissive).

## Relay channel allocation

| Relay | GPIO | Function |
|---|---:|---|
| CH1 | GPIO1 | Pump LOW contactor coil |
| CH2 | GPIO2 | Pump HIGH contactor coil |
| CH3 | GPIO41 | Heater safety contactor coil (fed via high-limit + LOW spare pole 1) |
| CH4 | GPIO42 | UV / ozone (firmware: only while pump LOW or HIGH runs) |
| CH5 | GPIO45 | Spa light |
| CH6 | GPIO46 | Spare / future |

## Currently free GPIOs

Exposed on the header and unassigned:

| GPIO | Notes |
|---:|---|
| GPIO35 | free — usable because the N16 module has no octal PSRAM (would be taken on an N16R8 / N8R8 variant) |
| GPIO36 | free — same note as GPIO35 |
| GPIO37 | free — same note as GPIO35 |
| GPIO39 | free (JTAG MTCK only if pad JTAG is enabled; default uses USB JTAG) |
| GPIO40 | free (JTAG MTDO, same note) |
| GPIO47 | free |
| GPIO48 | free |
| GPIO3 | strapping pin (JTAG source select) — use only for something harmless at boot, e.g. an input with no external pull |

Do not allocate any permanently without checking:
- Waveshare schematic
- boot/strap behavior
- onboard peripheral conflicts
- voltage compatibility

## Rules

1. No user-interface code may directly control mains outputs.
2. Matter, web UI and topside button handlers only submit requests to the safety state machine.
3. GPIO assignments in firmware must be defined in one central config file.
4. Any pin change must be reflected here before firmware is merged.
5. The old VL260 RJ45/data/clock/button-emulation GPIO allocation is retired (see `archive/`).
