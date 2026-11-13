# Bill of Materials

## Ordered / selected

| Qty | Part | Notes |
|---:|---|---|
| 1 | Waveshare ESP32-S3-Relay-6CH (ESP32-S3-WROOM-1U-N16) | Main controller |
| 1 | 24 VDC DIN PSU, 60 W / 2.5 A | Control supply |
| 2 | CJX2-K1201 DC, 24 VDC | 12 A AC-3 pump contactors, 3NO + 1NC; LOW contactor's two spare NO poles are the hardware heater permissive |
| 1 | JOTTA SSR-40DA or equivalent | 40 A nominal, zero-cross heater SSR |
| 2 | Molex 50-84-2042 | 4-position female cable housing |
| 1 | Molex 50-84-2032 | 3-position female cable housing |
| 1 | Molex 50-84-2022 | 2-position female cable housing |
| 20 | Molex 02-08-1002 | female crimp contacts |
| 1 | 12 V AC power supply | spa light, switched by CH5 |
| assorted | Ferrules | 0.5 / 0.75 / 1.0 / 1.5 / 2.5 mm² |

## Still to finalize / purchase

| Qty | Item | Notes |
|---:|---|---|
| 1 | Waterproof enclosure | size after bench layout |
| 1 | Heater safety contactor | 2-pole, 20–25 A AC-1 or better, 24 VDC coil |
| 1 | Independent heater high-limit | manual reset preferred |
| 1 | SSR heatsink | sized for continuous ~8.3 A heater load |
| 1 | Thermal compound | SSR to heatsink |
| 2–3 | 24 V coil suppressors | pump contactors + heater safety contactor |
| assorted | DIN rail | 35 mm |
| assorted | DIN terminal blocks | active / neutral / PE / 24 V / 0 V |
| 1 | Earth bar / PE distribution | |
| 1 | Neutral distribution block | |
| 1 | Active distribution block | finger-safe |
| assorted | branch protection | small breaker / fuse for control circuits |
| assorted | IP-rated cable glands | |
| assorted | internal mains wire | final gauge to suit circuit and terminals |
| assorted | 24 V control wire | |
| short length | mains-insulation-rated wire for 24 V permissive runs | for the two 24 V circuits landed on the LOW contactor's spare main poles |
| 1 | GC9A01 1.28-inch round 240×240 SPI TFT | replacement topside |
| 4 | waterproof momentary push buttons | Jets / Light / Temp+ / Temp− |
| 1 | sealed window / gasketed bezel for TFT | display module is not waterproof |
| 1 | topside cable, ≤ 400 mm | SPI + 4 buttons + 3.3 V + GND (≥ 13 cores incl. extra GND) |
| 4 | 10 kΩ resistors + 4 × 100 nF capacitors | button pull-ups / filtering |
| 1 | ADS1115 16-bit ADC module | M7 sensors A0/A1, 3.3 V reference A2 |
| 2 | 10 kΩ 0.1 % resistors | M7 divider references |
| 3 | 1 kΩ resistors + 3 × 100 nF capacitors | ADC input RC filters (A0, A1, A2) |
| 1 | DS3231 RTC module with CR2032 / LIR2032 | board has no RTC; I²C 0x68 |
| 1 | low-side MOSFET module or BC337 + resistors | SSR input driver from GPIO16 |
| optional | ~2.2 kΩ 1 W resistor | SSR input bleed if LOW spare pole 2 is unreliable at ~10 mA |
| optional | pump current sensor | enhanced diagnostics |
| optional | heater current sensor | enhanced diagnostics |
| optional | SSR temperature sensor | enhanced diagnostics |

## Future chemistry

- isolated pH interface
- isolated ORP interface
- pH probe
- ORP probe
- sample flow cell
- dosing pump(s)
- chemical tank level sensing
