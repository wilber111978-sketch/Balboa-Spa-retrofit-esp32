# Demo UI milestone 1 — executed validation

> Historical validation at the pre-integration checkpoint `bb305cc`. Current live-adapter results are in [LIVE_API_TEST_REPORT.md](LIVE_API_TEST_REPORT.md).

Date: 5 November 2026. Demo UI and reviewed V0.6.2 firmware are separate layers. No live controller or mains equipment was connected.

| Check | Executed result |
| --- | --- |
| Node unit/service/render suite | **33 passed, 0 failed** |
| Chromium browser interaction/layout suite | **14 passed, 0 failed** |
| Firmware suite from repository location | **141 passed, 0 failed** |
| Static app payload budget | **54,454 bytes raw; 18,795 bytes gzip**, below the 100 KiB raw budget |
| JavaScript syntax checks | Passed |
| Browser console errors / unexpected external requests | None in the executed suite |

## Unit/service coverage

- All 12 scenario fixtures produce contract-shaped, coherent state.
- Fault, Service Mode and HIGH pump states remove simulated heating.
- OFF/LOW/HIGH requests map to high-level service commands.
- Target bounds, finite values and 0.5°C steps; valid schedules including overnight/equal windows and invalid days/times.
- Fault/service guards inside the demo service, independent of disabled buttons.
- Browser-local persistent Service Mode, failed saves and watchdog acknowledgement/exit order.
- Offline/stale guards, all four screen renderers, detailed fault records and escaped untrusted text.
- Bounded session history and immutable captured snapshots.
- No relay/GPIO/SSR/contactor request route; unexpected fields rejected.
- Planned HTTP transport: safe JSON routes, explicit acknowledgement, authoritative state refresh, rejected/unacknowledged requests, abort timeout and accepted-but-unverified readback failures.

## Browser checks

The final browser suite exercised:

1. Local demo load with expected default temperature and 11:00–16:00 schedule.
2. Confirmed target and HIGH jets requests; heater command remains OFF in HIGH.
3. Rejected command visibility and unchanged target.
4. The 12 scenarios across scenario navigation and the watchdog recovery test.
5. Offline banner and disabled starts.
6. Diagnostics OEM-style code, internal ID, trigger, captured values, causes and checks.
7. History → full fault detail dialog.
8. Schedule edits surviving polls, then saving overnight times/weekdays.
9. Service entry confirmation, cancel and confirmed shutdown.
10. Service persistence across reload and explicit exit.
11. Watchdog recovery: acknowledge fault first, then exit Service Mode.
12. Paused updates become stale, disable requests and recover on resume.
13. Home/History/Diagnostics/Settings at 360, 390, 430 and 1280 px with no horizontal overflow.
14. No page/console errors or unexpected external dependencies.

The runner captured `docs/screenshots/home.jpg` and `docs/screenshots/diagnostics.jpg`; both were visually inspected. Node v24.22.1 and Playwright 1.62.1 were used. Browser validation ran with Chrome for Testing headless shell 134.0.6998.88 selected through `CHROMIUM_PATH`, because the bundled-browser download was unavailable and the newer downloaded Chrome crashed in this environment. No browser run is marked passed on that failed setup.

## Firmware evidence

The imported control headers are byte-identical to the reviewed V0.6.2 release that compiled for ESP32 core 3.3.2. Existing Main/Demo build logs and configuration are under `firmware/logs/`; no redundant full build was performed solely to move unchanged source into the repository. The complete 141-scenario suite and invariant checks were rerun successfully from `firmware/tests/run_all.py` after import. See `firmware/reports/TEST_REPORT.md` for exact compiler/memory results and hardware limits.

## Remaining work

- Implement the planned rich JSON API and bounded static-file serving in firmware through a separately reviewed integration change.
- Negotiate live capabilities, authentication, schedule precision/weekdays, target increments and safe `NORMAL` semantics. The UI's HttpSpaService is disconnected by default and does not pretend to talk to V0.6.2's form-command endpoint.
- Test on David's Android phone/tablet and the real LAN. No Safari or physical-device acceptance is claimed.
- Validate electrical interlocks, flow, independent thermal protections, watchdog/reset behavior and sensor calibration on isolated hardware. UI simulation does not establish electrical safety certification.
