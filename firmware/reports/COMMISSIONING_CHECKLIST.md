# Hardware safety and commissioning checklist — V0.6.2

**Status: NOT approved for live heating. Both builds physically inhibit CH1 and CH3.**
Compilation and simulation do not establish electrical safety certification. No hardware was connected during this review. Keep the heater isolated during bench work; software inhibition does not prove that a contact is open or prevent a welded contact or wiring error.

## Board and relay identification

- [ ] Confirm the actual board revision/module matches Waveshare ESP32-S3-Relay-6CH, and verify its flash size before selecting the 16 MB build configuration.
- [ ] With isolated test loads, identify CH1 GPIO1 (heater safety contactor), CH2 GPIO2 (LOW), CH3 GPIO41 (SSR control), CH4 GPIO42 (HIGH), CH5 GPIO45 (ozone/UV), CH6 GPIO46 (light).
- [ ] Verify HIGH-active relay polarity, COM/NO/NC terminals and OFF behavior. Check power-up, reset, bootloader entry, brownout and USB-only power with instruments; firmware cannot control GPIO before its startup executes.
- [ ] Confirm SDA GPIO8 / SCL GPIO9 are accessible and wired to the ADS1115 at address 0x48. Verify 3.3 V divider supply, ADC supply, common reference and isolation arrangement. No mains voltage at sensor or logic inputs.

## Pump contactors and motor protection

- [ ] Have the electrician verify the two-speed motor's approved switching arrangement and whether the proposed contactors are suitable. Board relay contacts are commands, not independent proof of motor isolation.
- [ ] Provide electrical cross-interlocking using the appropriate normally closed auxiliary contact of each opposite contactor; use a mechanically interlocked pair where applicable to the selected contactors/motor arrangement.
- [ ] Verify coil voltage and AC/DC type, suppression, protective devices and contact ratings for motor starting/inrush. Do not infer motor suitability from a resistive relay rating.
- [ ] Measure release and pickup times. Confirm 750 ms software deadtime is adequate with the actual contactors, suppression and supply tolerances.
- [ ] Test LOW→HIGH, HIGH→LOW, rapid conflicting commands and loss of coil power; prove both motor windings cannot be energised together even if software or a driver fails.
- [ ] Determine the required auxiliary feedback and welded/stuck-contactor response. Firmware feedback pins are unassigned and no release/flow verification currently exists.
- [ ] Confirm motor thermal/overload protection and the required safe fault response. Current firmware stops LOW, HIGH and ozone/UV on every fault; there is no emergency cooling circulation strategy.

## Independent heater protections — required before a future live-heating revision

- [ ] Validate independent, suitably rated overtemperature protection that removes heater energy without the ESP32, Wi-Fi or either temperature calculation functioning.
- [ ] Validate independent circulation/flow or dry-fire safeguards. Commanded LOW time and two agreeing thermistors do not prove water flow or an adequately filled heater tube.
- [ ] Verify the series safety contactor and SSR power path, failure modes, disconnect means, current ratings, thermal design, SSR heat sink and off-state leakage. Check both shorted-SSR and welded-contactor faults.
- [ ] Confirm that the independent safety chain removes energy on broken wires, supply loss and controller failure. The software high-limit input is currently unassigned; no physical high-limit input has been commissioned.
- [ ] Verify the OEM probe resistance curve, beta, tolerance, physical placement and calibration across the operating range. The 30 kΩ / beta 3860 model is an assumption, not established compatibility with the installed probes.
- [ ] Validate temperature agreement, raw 42°C trip, slope limits, circulation qualification and postheat behavior with a filled, vented hydraulic system. No certified Balboa M7 algorithm has been reproduced.
- [ ] Check earthing/bonding, RCD protection, isolation, enclosure, moisture protection, conductor sizing, protective devices and applicable installation requirements with the responsible qualified installer.
- [ ] Validate the whole-system current budget, including simultaneous pump/heater operation, against the existing supply and plug rating.
- [ ] Do not enable heating by changing a single flag: this release has an unconditional CH1/CH3 output gate as well as the Main commissioning flag. A future live-heating release requires a separately reviewed design, implemented safety inputs/feedback and physical validation.

## Water management, recovery and network

- [ ] Validate the four-hour LOW quota and ozone/UV duty cycle against the actual spa, filter, treatment system and manufacturer instructions. Four hours and the 56-hour/14-day cleaning reminder are development defaults, not sanitation approval.
- [ ] Verify that ozone/UV is appropriate whenever this code permits it; prove actual circulation and any bather restrictions required by the equipment.
- [ ] Test one full jets/after-use cycle: HIGH ends → 60 minutes accumulated LOW plus ozone/UV, paused during HIGH/faults. Confirm resumed requests after power loss are acceptable.
- [ ] Test real power cuts, flash/NVS errors and resets in every operational state. Confirm fault latches persist; manual LOW/HIGH/Ready/manual heat do not resume. Solar scheduling and saved after-use requests may resume after a healthy restart.
- [ ] Establish the filter-clean date using `filtercleaned confirm` with valid time. Calendar age is unknown until a valid clean date is saved.
- [ ] Verify Sydney standard/daylight time, first NTP sync, internet loss, clock correction, midnight and overnight windows. No clock means solar heating/filtration scheduling fails closed, but explicit LOW/after-use/manual demand can still request circulation.
- [ ] Configure unique Wi-Fi/HTTP credentials; keep the controller on a trusted isolated LAN. HTTP Basic authentication is unencrypted. Do not expose its HTTP port to the internet.
- [ ] Test actual disassociation/reassociation, weak RF, stalled HTTP clients and concurrent commands. Mock tests do not measure ESP32 network latency or prove a control-loop timing bound.
- [ ] Verify a local isolation/service procedure. `off` releases the manual pump request; it is not an emergency stop and automatic filtration, heat demand or after-use can request LOW again. `alloff` now enters persistent Service Mode; verify all six outputs stay OFF and schedules stay blocked across restart until `service off confirm`. Light is independently switched and remains available during faults.
- [ ] VL260 topside integration, freeze protection and contactor/current feedback remain unimplemented. A dedicated 5-second application watchdog is now configured; measure real expiry, reset reason, output drop-out and Service Mode recovery on the isolated board.

## V0.6.2 acceptance additions

- [ ] Enter `alloff` with each load active; verify all six physical contacts release. Cycle power and prove Service Mode remains enabled. Verify remote LOW/HIGH/light requests are refused.
- [ ] Clear a safe sensor fault, then separately exit Service Mode with `service off confirm`. Prove no fault reset alone starts a load or exits Service Mode.
- [ ] Induce a controlled loop stall on the isolated test board; measure watchdog timeout/reset and verify boot enters Service Mode with a latched reason. Test panic and brownout recovery independently.
- [ ] Test real NVS save failure/brownout during blob writes; verify the old or new complete blob is accepted, damaged records fail closed and failed operations are not acknowledged as saved.
- [ ] Stall header/body delivery and response consumption; measure control-loop and sensor-sampling latency during real Wi-Fi driver activity. Software byte/deadline limits do not establish a hard system-wide execution deadline.
- [ ] After a loop delay beyond 2 seconds, prove stale data faults before acquisition can refresh the old timestamps. Both heater pins must remain inhibited throughout.
