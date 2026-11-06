# Host topside tests and screen mock

Run from the repository root:

```sh
export ARDUINOJSON_SRC=/absolute/path/to/ArduinoJson/src
bash tools/test-topside.sh
```

This verifies the44 frozen files, runs42 topside cases across Main/Demo, the177 controller/API cases through the new wrapper,40 existing UI tests and asset budget, and emits six SVG visual mocks from the shared strip painter. Tests use actual controller functions with the existing mocked peripherals/NVS/time/sockets. They do not require a display or mains.

For targeted development only:

```sh
python3 firmware/topside-tests/run.py
```

Each new scenario has explicit `require` expectations in `scenarios.cpp`. `results.json` records each executed result. In Demo, `topsideSimulateButtons(mask)` accepts Jets1/Light2/Up4/Down8 using the same40ms/800ms/repeat state machine; advance mocked milliseconds between samples. No hook is compiled into Main. `render-svg.cpp` emits Normal/Adjust/Fault/Service/Invalid/Info using the actual model and strip layout. SVG fonts are approximate host fonts, not pixel-exact GC9A01 font captures.

Optional existing browser suites: install the repository Playwright dependency/browser, set `RUN_BROWSER=1`, and rerun only the browser commands if C++/UI checks have already passed. `PLAYWRIGHT_MODULE` and `CHROMIUM_PATH` permit an existing installation. Browser checks are14 Demo +8 native C++ live API checks. The slow renderer test uses a separate mock display thread blocked300ms while the actual controller loop processes an overtemperature fault. Real display init/SPI interruption/latency/disconnection remain bench tests.
