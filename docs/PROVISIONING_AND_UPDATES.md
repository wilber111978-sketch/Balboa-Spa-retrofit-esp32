# Provisioning, Firmware Updates and Flash Storage

Covers three things that only show up once the controller leaves the bench:

1. getting a new (or reset) controller onto Wi-Fi and pairing the web app
2. updating firmware without opening the enclosure
3. storing settings and logs without wearing out the flash

The spa must work fully with none of this in place: no Wi-Fi, no app and no updates are ever required for local operation.

---

## 1. First-time setup (provisioning)

### Primary path — Google Home / Matter

Matter commissioning over Bluetooth (BLE) hands the controller the home Wi-Fi details during pairing. One step does both:

1. Open the topside Setup screen (hold **Jets + Light** for 3 s). It shows the Matter pairing QR code.
2. In the Google Home app: Add device → Matter → scan the QR code.
3. Google Home passes the Wi-Fi network and password to the controller and pairs it.
4. The Setup screen then shows the controller's address and the web app QR code.

The Matter QR code and manual pairing code should also be printed on a label inside the enclosure lid.

### Fallback path — setup access point

Used when Google Home is not used, or Matter pairing fails.

- If the controller has no Wi-Fi details, or cannot join its saved network for 5 min after boot, it starts its own Wi-Fi network: `FisherSpa-XXXX` (XXXX = last 4 of the MAC), WPA2 with a password shown on the Setup screen.
- The Setup screen shows a QR code to join that network.
- Joining it opens a captive setup page: choose your home network and enter its password.
- The controller joins the home network and shuts the setup network down.
- The spa keeps running normally throughout.

### Finding the controller

- mDNS name: `http://fisherspa.local`
- IP address shown on the Setup screen
- a static DHCP reservation in the home router is recommended

### Web app token

All state-changing web API calls need a bearer token (see [API_CONTRACT.md](API_CONTRACT.md)).

- Generated on first boot and stored in NVS.
- The Setup screen shows a QR code for `http://<address>/#token=<token>`. Scanning it opens the app, which saves the token in the browser and removes it from the address bar.
- New token (all browsers must re-pair): on the Setup screen, hold **Temp−** for 5 s.

### Reset Wi-Fi

On the Setup screen, hold **Jets** for 10 s: clears saved Wi-Fi details and restarts in setup access-point mode. Settings, schedules, fault history and Matter fabric data are kept; "Matter pairing/reset" in the app clears Matter separately.

---

## 2. Firmware updates

The ESP32 has no SSH or shell, so there is nothing to SSH into. Updates are done by replacing the firmware image. Three paths, in order of normal use:

| Path | Who | How |
|---|---|---|
| **Web app upload** (normal) | owner | Settings → Firmware update → choose the `.bin` file → confirm |
| **Network upload from laptop** (development) | developer | PlatformIO / `espota` upload over Wi-Fi; disabled by default, enabled from the app for 15 min at a time, password protected |
| **USB-C** (recovery) | developer | board's USB-C port; route a short USB-C extension to an accessible point inside the enclosure lid so recovery does not need the controller unmounted |

### How an update runs

The flash has two firmware slots (A/B). The running firmware is never overwritten.

1. Upload is accepted only when the heater is off and the controller is not priming or in fault lockout. Otherwise `409` with the reason.
2. The new image is written to the inactive slot while the spa keeps running. Size, chip type, project name and SHA-256 are checked.
3. Before restarting: heater-off sequence, pump OFF, light OFF, event logged.
4. Restart into the new firmware. The reset reason is "software / OTA", so priming is skipped (the water was not drained).
5. **Self-test** (first ~60 s): I²C devices respond, both sensors in range, state machine running, web server up, no watchdog reset.
6. Pass → the new image is marked good.
   Fail, or it crashes/reboots before marking good → the bootloader automatically rolls back to the previous image and FIRMWARE_ROLLBACK is logged.
7. The app can also roll back manually to the previous image.

### Web app files

Release builds embed the gzipped web app (`index.html`, `app.js`, `styles.css`) inside the firmware image, so one `.bin` updates firmware and app together and they can never be out of step. During development the files may be served from LittleFS for quick iteration.

### Later options

- signed firmware images (ESP-IDF secure boot / signed OTA) once the design settles
- Matter OTA (updates offered through the Matter ecosystem) — not needed while the owner builds the firmware

---

## 3. Flash partition plan (16 MB module)

Approximate; finalise in `partitions.csv` once the Matter build size is known.

| Partition | Size | Use |
|---|---|---|
| nvs | 192 KB | settings, Wi-Fi details, token, latched faults (wear-levelled by NVS) |
| otadata | 8 KB | which firmware slot is active |
| phy_init | 4 KB | RF calibration |
| fctry | 24 KB | Matter factory data (pairing code, discriminator) |
| app0 (ota_0) | 4 MB | firmware slot A |
| app1 (ota_1) | 4 MB | firmware slot B |
| storage (LittleFS) | ~7.6 MB | event / fault log, dev-time web files |
| coredump | 64 KB | crash dump for diagnostics |

A Matter-over-Wi-Fi build with web server and display is typically 1.5–2.5 MB, so 4 MB slots leave room to grow.

---

## 4. Flash wear — recommendations

ESP32 flash sectors are rated for roughly 100,000 erase cycles. NVS and LittleFS spread writes across their whole partition, so wear only becomes a problem if something writes continuously. The rules below keep total writes to a few hundred per day, which spread across these partitions works out to many decades.

### What to persist

| Item | When written |
|---|---|
| setpoint | **5 s after the last Temp+/− press**, and only if the value changed (a run of presses = one write) |
| heating mode, schedules, filter cycles, economy floor | on explicit Save in the app, only if changed |
| light program count / timing, service settings | on Save |
| Wi-Fi details, token, Matter data | on change only |
| latched critical faults | immediately when latched and when reset, so a power blip cannot clear an OVER_TEMP lockout (local reset still required) |
| event / fault log | append one fixed-size record per event (fault, state change, heater start/stop, reset, update) |
| run-hour counters (pump LOW, pump HIGH, heater, UV) | kept in RAM, written hourly and before planned restarts |

### What never to persist

- live temperatures, sensor samples, ADC values
- pump / heater / UV output state (outputs always start OFF; the state machine rebuilds state after boot)
- the last water temperature (after power-up the display shows `--.-°` until a fresh reading)
- light on/off state (light is OFF after power-up)

### Event log

- fixed-size records (~64 bytes) in a ring file on LittleFS, e.g. 1 MB ≈ 16,000 events
- oldest records overwritten when full
- a busy day (filter cycles, heating cycles, jets use) is in the tens of events, so the log holds years of history

### Power-loss safety

- NVS writes are atomic: a power cut during a write leaves the old value intact
- LittleFS is power-loss tolerant; a torn final log record is discarded on the next boot
- never write flash from an interrupt or from the safety loop itself; queue writes to a low-priority storage task

---

## Topside button combinations (summary)

| Combination | Hold | Action |
|---|---|---|
| Temp+ or Temp− during priming | press | end priming early |
| Temp+ and Temp− | 5 s | reset latched safety fault |
| Jets and Light | 3 s | open Setup screen |
| Jets (on Setup screen) | 10 s | clear Wi-Fi, restart in setup access-point mode |
| Temp− (on Setup screen) | 5 s | new web app token |
