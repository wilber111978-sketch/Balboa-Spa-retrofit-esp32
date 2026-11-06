# V0.8 topside implementation and operation

`SpaTopside` is an input/display adapter. The frozen controller remains the sole owner of relays, scheduling, heating, circulation, faults and persistent settings. `SpaMain_Live` and `SpaDemo_Live` now include the same `SpaTopside.h`. A small opt-in entry-point wrapper in `SpaLiveApi.h` preserves its original standalone behavior while exposing `spaLiveSetup`/`spaLiveLoop`. JSON state/API shape and dashboard assets are unchanged; firmware reports0.8.0-topside-main/demo.

## Input policy

All buttons are normalized into a four-bit pressed mask. New presses are accepted only after100ms all-released at boot/rearming. Debounce is40ms. Short actions fire on release. Long is detected at800ms; only temperature keys then repeat at250ms, at most one event per poll, with no catch-up burst. Light long opens read-only information; Jets long does nothing. Releasing a long press never emits another short action.

Simultaneous or overlapping buttons suppress the whole gesture until all release. At10s a held input becomes stuck, displays a release warning and suppresses further events until a clean release/rearm. Boot-held buttons never generate commands. These UI input policies neither create controller faults nor reset them. Existing pump timers/thermal protection continue independently if a user-interface input fails. A sustained temperature hold can make validated0.5C adjustments up to the existing limit before the stuck timeout; it cannot override schedule/heat safety.

All normal requests are rejected in Service, any existing fault or settings-integrity lockout. `executeCommand` handles Jets low/high/off, Light and setpoint. The API's250ms non-stop command limiter is shared; stopped manual pump/turning off light need not wait. There are no raw relay calls, fault-reset gestures, Service-exit gestures or heating-mode/schedule commands. Rejection is shown on-screen; exact fault detail remains in the dashboard. Commands retain the controller's normal Ready/after-use semantics. OFF is not full shutdown.

## Screen behavior

- Normal: fresh measured temperature, target, actual pump OFF/LOW/HIGH, Wi-Fi, heating mode and explicit physical heating inhibition. Demo logical heat demand is labeled as such.
- Adjustment: current selected target in0.5C steps; five seconds after the last button interaction returns to Normal. At a limit the target is still shown with rejection feedback. No separate unsaved target copy exists.
- Fault: existing numeric fault plus shared API identifier/title; no invented fault IDs. Fault wins over Service, adjustment and information; concurrent Service remains labeled. Service alone shows STOPPED and heating inhibition. Invalid/nonfinite/stale measured temperature shows`--.- C`; snapshot reads status/timestamps without invoking fault-mutating validation helpers.
- Long Light information: version/commissioning notice and direction to the existing dashboard for settings. No local safety settings are editable.

## Display isolation

Pinned graphics driver: Adafruit GC9A01A1.1.1 + Adafruit GFX1.12.1 + BusIO1.17.4. No LVGL. The worker initializes a dedicated FSPI bus on the declared pins and writes a240x8 RGB565 strip (3840bytes), rather than a115200-byte framebuffer. Reset delays and SPI drawing occur only in a core0 FreeRTOS task at priority1, with a6144-byte stack. It yields between strips; unchanged screens are skipped and refresh polling is200ms. Screen-priority transitions restart drawing from the top at the next strip boundary. GUI refresh latency is not controller fault latency.

The controller loop runs `spaLiveLoop()` first, processes bounded input events, builds a fixed-size value snapshot, and publishes under a short critical section. No SPI, driver init, wait, rendering or display acknowledgements occur under that lock or on the controller path. No per-frame heap allocations are introduced. Existing command Strings/NVS writes retain their established behavior; fixed strip/task allocations occur once. A blocked renderer cannot hold the controller snapshot lock because drawing takes place after copying it.

Main defaults to actual GPIO inputs and the display worker. Demo defaults to no hardware task or topside GPIO access and exposes a C++ `topsideSimulateButtons()` hook only when `SPA_BENCH_DEMO=1`. The host scenarios exercise the same debounce/command/model/painter. There is no new serial/HTTP mock-input route in Main. `SPA_TOPSIDE_HARDWARE=1` can opt Demo into isolated hardware testing;`=0` can disable peripheral I/O for Main diagnostics. Backlight stays OFF until the external driver verification flag is enabled; see hardware document.

## Known peripheral limits

The specified four-wire SPI bus has no MISO/presence input. Disconnection is therefore not reliably detectable, and no controller fault is invented for a missing screen. Initialization has no useful connected-panel return value in the selected library. The host disconnected mock verifies controller independence, not physical cable detection/recovery. A physical module losing power may need reinitialization; reconnect behavior is a bench item. Core watchdog/fault handling remains independent. Worker allocation or SPI failure can leave the display blank/stale while the web UI and safety controller continue; never use a frozen picture as evidence of current output state.
