# Hardware Inventory — Master

## Arrived / physically in hand

- Waveshare ESP32-S3 Relay-6CH
- 24 VDC DIN power supply, MDR-60-24 style, 60 W / 2.5 A
- 2 x CJX2-K1201 DC pump contactors
- JOTTA SSR-40DA zero-cross SSR
- SSR aluminium heatsink
- ferrule assortment
- ferrule crimping tools
- modular/RJ-style breakout / termination adapter for low-voltage panel work
- Molex spa harness connector parts / crimp contacts as ordered
- 12 V AC power supply for the spa light (switched by CH6)

## Ordered / pending or to confirm

- heater safety contactor, 24 VDC, 25 A, 2NO
- waterproof enclosure
- independent heater high-limit device
- DIN terminal/distribution blocks
- coil suppressors
- ADS1115 module: NOT yet purchased. V0.9 runs on the onboard ADC until it is fitted (one build flag)
- independent high-limit, manual reset, 44 °C, with a second contact or opto for the status input (GPIO16)
- thermal fuse ~70–75 °C, mains-rated, for the heater body
- 2 x 30 kΩ 0.1 % divider resistors, 1 kΩ + 100 nF input filters
- GC9A01 round TFT + 4 waterproof buttons (V0.8 topside)
- low-side MOSFET module for SSR drive (existing farm-sensor spare may be reused after verification)

## Bench-layout note

Enclosure size will be selected only after all major hardware is laid out on DIN rail with:
- cable bend radius
- SSR heatsink clearance
- mains / low-voltage separation
- terminal blocks
- service loops
- future expansion margin
