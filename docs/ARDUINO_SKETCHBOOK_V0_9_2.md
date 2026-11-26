# V0.9.2 Arduino sketchbook notes

This is the README_FIRST.txt shipped in the local Arduino-ready sketchbook. The sketchbook's
`SpaMain_Live/SpaOtaCredentials.h` and the Wi-Fi lines in the sketches hold passwords and are
never committed. `SpaOtaCredentials.h` contains one line:
`#define SPA_OTA_PASSWORD "<long unique password>"`.

```text
BALBOA SPA RETROFIT V0.9.2 - DEVELOPMENT SKETCHBOOK
=====================================================

This folder is a complete Arduino "sketchbook": the two sketches plus every
library they need, already in the right places. Nothing needs copying.

V0.9.2 DEVELOPMENT STATUS
- This copy is based on the installed V0.9 Arduino sources.
- The live dashboard has numbered light programs. Check the actual
  colour/effect order before naming the programs.
- Diagnostics can download the last ten minutes of A/B probe readings and
  heater commands observed by the open dashboard. This is a 1.5-second
  polling log for circulation checks, not a fast commissioning instrument.
- Both physical heater outputs are inhibited in the supplied build.
  The live M7 control path is staged behind the commissioning profile.
- Dashboard changes passed local checks with simulated API responses.
  The Arduino CLI could not access the installed ESP32 platform here, so
  verify the sketch in Arduino IDE before uploading. The first installation
  of V0.9.2 requires USB.
- This local copy retains the existing sketch credentials. Keep it private.

INSTALLATION NOTES REPORTED BY OWNER
- The pump, temperature readings and other installed non-heater functions
  have been tested. The topside is being built for installation by flashing.
- The retrofit has a 15 A breaker. The 10 A outlet has earth-leakage
  protection and an additional 20 A breaker. Pump HIGH runs alone.
  The owner reports the original GS100 spa ran with its factory loads
  on the same 10 A outlet for six years, and the retrofit retains them.
  The published Fisher 3 specification pairs a 1.5 kW heater with a
  10 A supply. It describes the current model and does not identify
  the older GS100 board revision.
  https://www.spaworld.com.au/spa-pools/fisher/fisher-3/
- The owner corrected the heater rating to 1500 W. If the previously
  reported 220 V nameplate voltage also applies, estimated element
  current alone is 6.8 A at 220 V, 7.1 A at 230 V and 7.4 A at 240 V,
  assuming approximately constant resistance.
  The wiring drawing's earlier heater-current estimate needs updating.
  Verify the combined LOW pump, heater and auxiliary load against the
  installed plug and outlet before a powered pulse. The 15 A and 20 A
  breakers do not change the 10 A outlet rating.
- No powered heater trace is available for the ESP32 retrofit. The
  owner reports installing a normally closed, manual-reset 45 C switch
  on the heater body. It interrupts the 24 V control supply, including
  the ESP32. The separate high-limit status input remains unassigned.
  The 3.3 V reference and 24 V monitor remain disabled.
- The owner reports the isolated cutoff test dropped 24 V to the heater,
  LOW, HIGH, ozone and ESP32. Record the heater contactor's main-contact
  dropout and retention until manual reset on the commissioning sheet.
  A loss of 24 V also resets the ESP32,
  so firmware cannot distinguish a trip from a normal power outage.
  Both heater outputs remain inhibited in this development build.

HEATER RESTART HOLD
- Main holds heater permission after every restart, including after OTA.
  The hold is independent of Service Mode and fault acknowledgement.
- After checking the spa, acknowledge it under Settings in the web
  dashboard. A restart does not identify a 45 C cutoff trip.
- Event history records the restart hold. When the ESP32 loses power
  before seeing a temperature fault, no overtemperature event is assumed.
- This acknowledgement clears only the restart hold. The existing
  physical relay inhibition is still present in this build.

M7 QUALIFICATION MODEL
- M7Qualification.h uses a three-minute circulation stage before capturing
  an A/B baseline, then models a bounded pulse and a 90-second heater-off
  observation. Both probe channels are treated symmetrically.
- The model requires explicit pulse and temperature limits. It rejects
  invalid or stale readings, interlock loss, mismatched baseline, excess
  heat, no response and failure to recover. It has no relay access.
- Reference: Balboa 50 Hz M-7 Technical Service Manual, page 25,
  https://hydrospares.2dimg.com/resources/1471442671_3869.pdf
- M7HeaterCycle.h adds the contactor proof, timed SSR output, heater-off
  observation and handoff to the thermostat. The first powered profile
  is intended for pulse-only operation. Ongoing heating requires a
  second commissioning check.
- The supplied sketch has no powered profile. Set pulse and probe limits
  from the installed heater rating and commissioning data, and verify
  the independent shutoff with an isolated element before using a
  powered profile. The Balboa manual gives model-dependent pulse timing.

WI-FI UPDATE STATUS
- The documented 16 MB Arduino partition option has two 3 MB application
  slots in the installed ESP32 board package. Keep that partition option
  when building future images. Keep USB accessible for recovery.
- Once V0.9.2 is installed, enter Service Mode on the dashboard, then
  select Settings > Firmware update > Open upload window. Select the
  fisher-spa network port in Arduino IDE and upload SpaMain_Live using
  the password in SpaMain_Live/SpaOtaCredentials.h.
- Upload is intended for application firmware only, on a trusted local
  network. The update window lasts up to ten minutes. All outputs are held
  off in Service Mode. Service Mode is sticky across a restart and can be
  exited after verification or when the listener closes.
- On the first boot of an updated image from this firmware family, the
  controller checks its sensor readings, Wi-Fi, watchdog and Service Mode
  before confirming the new image. A failed check should roll back.
  The upload, interrupted transfer and rollback need verification on
  the actual board before relying on Wi-Fi as the normal update path.

ONE-TIME SETUP (Arduino IDE 2.x)
1. File > Preferences > "Sketchbook location": Browse... and choose THIS folder
   (the one containing this README), e.g. the Spa_V0.9.2_Arduino workspace folder
   Click OK, then close and reopen the Arduino IDE.
   (To go back to your normal projects later, set it back to Documents\Arduino.)
2. Tools > Board > Boards Manager: search "esp32", install "esp32 by Espressif
   Systems" version 3.3.12 with application rollback support.
   Do NOT install the libraries again - they are already in the libraries folder.

BOARD SETTINGS (Tools menu)
   Board ............ ESP32S3 Dev Module
   Flash Size ....... 16MB (128Mb)
   Partition Scheme . 16M Flash (3MB APP/9.9MB FATFS)
   PSRAM ............ Disabled
   USB CDC On Boot .. Enabled
   Port ............. the COM port that appears when the Waveshare board is plugged in

FIRST BUILD
1. File > Open > SpaDemo_Live\SpaDemo_Live.ino
2. Optional: fill in the Wi-Fi lines at the top of the sketch (see comments).
3. Click Verify (tick). It should compile with no errors.
   Also open SpaMain_Live and click Verify, to confirm the Main build compiles.
   If either fails, copy the error text and send it.
4. Upload SpaDemo_Live (arrow button). Open Tools > Serial Monitor at 115200 baud.

WHEN THE ADS1115 IS FITTED (can be sent over Wi-Fi)
   In SpaMain_Live.ino uncomment both:
     #define SPA_TEMP_ADC_ADS1115 1
     #define SPA_RAIL_REF_FITTED 1     (sensor rail wired to ADS1115 A2)
   See docs/M7_SENSOR_INTERFACE.md for the wiring change.

WHICH SKETCH
   SpaDemo_Live - bench testing. Temperatures simulated; heater relays CH1/CH3
                  forced OFF in firmware; pump/UV/light relays DO switch.
                  Test with NO mains connected to the relays.
   SpaMain_Live - real sensors. Use only once the M7 sensors are wired
                  (GPIO8/GPIO9 dividers). Heater relays remain forced OFF until
                  formal commissioning.

FOLDER LAYOUT
   Spa_V0.9.2_Arduino\
     README_FIRST.txt
     SpaDemo_Live\SpaDemo_Live.ino
     SpaMain_Live\SpaMain_Live.ino
     libraries\
       SpaController, SpaLiveApi, SpaTopside   (project code, V0.9)
       Adafruit_ADS1X15 2.5.0, Adafruit_BusIO 1.17.4, Adafruit_GC9A01A 1.2.0,
       Adafruit_GFX_Library 1.13.0 (fonts folder omitted - not used), ArduinoJson 7.4.2 (official single-header form)

Source: GitHub wilber111978-sketch/Balboa-Spa-retrofit-esp32, branch dev/v0.9
(V0.9.2). Docs: docs/V0_9_CHANGES.md and docs/HARDWARE_INTERFACE.md.
```
