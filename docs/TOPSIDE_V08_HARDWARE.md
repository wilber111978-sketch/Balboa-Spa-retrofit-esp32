# V0.8 custom topside: GPIO and electrical requirements

The VL260 has been replaced entirely. V0.8 contains no VL260 protocol implementation. Start point: branch `dev/spa-v062-demo-app`, inspected HEAD `e17fdec0e59cce09cd1963c5e7c3273363c03028`. Frozen V0.6.2 remains unchanged.

## Checked allocation

Reviewed the current firmware and [Waveshare ESP32-S3-Relay-6CH schematic](https://files.waveshare.com/wiki/ESP32-S3-Relay-6CH/ESP32-S3-Relay-6CH-Sch.pdf). The proposed pins route to the extension headers, have no current firmware assignments, and do not overlap relay outputs, I2C8/9, RS48517/18, buzzer21, RGB38 or BOOT0. GPIO4–7/10–15 are not S3 strapping0/3/45/46. Actual board revision/header continuity still requires verification. No silent reassignment was necessary. GPIO10–15 supersede the old unused VL260 reservation.

| GPIO | Topside use | Connection requirement |
|---:|---|---|
|10|SPI SCLK|GC9A01 SCL/SCK;3.3V logic|
|11|SPI MOSI|GC9A01 SDA/DIN/MOSI; SPI data, not I2C|
|12|Display CS|active LOW; external pull-up for inactive boot|
|13|Display DC|3.3V data/command|
|14|Display RESET|active LOW, verify module requirements|
|15|Backlight enable|external active-HIGH driver enable ONLY|
|4|Jets|momentary normally-open switch to GPIO GND|
|5|Light|momentary normally-open switch to GPIO GND|
|6|Temperature UP|momentary normally-open switch to GPIO GND|
|7|Temperature DOWN|momentary normally-open switch to GPIO GND|

Central definitions: `firmware/libraries/SpaTopside/src/TopsideConfig.h`. Hardware SPI has no MISO. Use the board's **GPIO GND**, not an assumed isolated RS485 SGND. Do not connect to RS485 A/B or Ethernet. Verify display supply pinout: this brief specifies a3.3V module; do not infer5V tolerance from other GC9A01 boards. Confirm regulator capacity and total load, cable voltage drop and startup current. Keep SPI cable short;24MHz is the software test setting, not a validated cable-speed rating. Reduce only after waveform evaluation and record any new timing results.

Backlight: GPIO15 cannot supply LED current. Use a correctly rated external transistor/MOSFET/current-limited backlight driver with default-OFF pull-down, suitable gate/base resistor and the module's specified LED current limit. If the module has a built-in enable transistor, verify its ratings/polarity first. V0.8 initializes GPIO15 LOW and leaves it LOW unless built with `SPA_TOPSIDE_BACKLIGHT_VERIFIED=1`. That flag assumes an externally verified active-HIGH ENABLE input. It never authorizes direct LED supply. Verify no backfeeding when either controller or display is unpowered. Exact circuit/current values are intentionally unresolved until the actual module is inspected.

Buttons use internal INPUT_PULLUP and active LOW. Add suitable external3.3V pull-ups, protection/conditioning and shielding for a wet-environment cable after noise testing. The switch contact circuit is separate from any illuminated button's LED terminals. Never connect12/24V LED wiring to GPIO; use an appropriately protected separate LED circuit. No5V/24V signals may reach S3 GPIOs. Confirm waterproof rating and mounting/sealing of the complete assembly independently of the switch advertisement.

## Unchanged relay assignments and protections

| Channel | GPIO | Function |
|---|---:|---|
|CH1|1|Heater safety contactor, physically inhibited|
|CH2|2|Pump LOW|
|CH3|41|Heater SSR control, physically inhibited|
|CH4|42|Pump HIGH|
|CH5|45|Ozone/UV|
|CH6|46|Spa light|

Preserve independent electrical/mechanical LOW/HIGH contactor interlocks and feedback. Software exclusion/deadtime does not protect against welded contacts or miswiring. Heater commissioning remains blocked; independent high-limit, flow/temperature safeguards, SSR failure protection and contactor feedback still need separate validation. Topside buttons do not reset faults, leave Service, change schedules or enable manual/grid heating.

## Isolated physical validation checklist — NOT executed

1. Disconnect mains and all six relay loads. Photograph board/module revisions and continuity-map actual extension headers. Verify all GPIO assignments against that board revision.
2. Verify regulated3.3V supply, ground, display polarity, supply/current ratings, backlight circuit and switch/LED separation. Scope boot/reset with external CS pull-up and backlight default OFF. Confirm no output glitches/backfeed with one supply missing.
3. Boot Main with display disconnected: control/web/fault handling must continue. No physical cable-presence detection is implemented on this write-only bus. CS/MOSI/SCLK transfer success cannot prove a panel is attached. Reconnection must be tested; a module that loses power may require a controller/display reinitialization. Do not assume automatic reconnect recovery.
4. With the isolated display connected, check reset/init, rotation, colours, temperature decimals and layout on the round screen. Confirm backlight current independently before enabling the verified driver flag. Compare host SVG mocks for content/geometry; fonts are approximate in those mocks.
5. Test each active-LOW button:40ms debounce,800ms long press,250ms temperature-only repetition,10s stuck suppression. Test simultaneous inputs, water/noise-induced shorts, boot-held inputs and release rearming. Holding Jets/Light must not repeat an output command. Light long press only opens information.
6. Verify target20–40C/0.5C steps and five-second inactivity timeout, command rejection text, live dashboard consistency and API/panel rate limiting. Jets OFF releases manual demand; automatic LOW/after-use circulation may resume. Use the existing confirmed web Service command for full shutdown.
7. Under LOW/HIGH/temperature requests and flooded/lost Wi-Fi, inject Demo sensor/stale/overtemperature faults. Fault/Service screens must take priority; CH1/CH3 stay off. Observe LOW/HIGH never overlap through transitions. Panel status must not claim heating is physically enabled.
8. Measure control-loop timing during display initialization, every strip transfer, disconnect/short/noisy SPI and render stall. SPI/reset/drawing run in a separate core0 task; controller runs first on its existing loop. Observe sensor freshness/watchdog and fault output-removal latency. A host timing test does not establish worst-case electrical timing.
9. Test power interruption, brownout, watchdog recovery and persisted Service. No button may clear or bypass recovery lockouts. Check memory headroom during Wi-Fi and rendering: worker stack6144 bytes plus3840-byte strip buffer and driver allocations occur once, beyond static RAM shown by the compiler.
10. Record measured results/captures, failures and module revision. Qualified electrical commissioning remains separate. Compilation/simulation are not electrical safety certification.
