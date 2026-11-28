#pragma once
// Optional Google Home / Matter bridge (off unless the sketch defines SPA_MATTER 1).
//
// Exposes exactly three things:
//   * Spa temperature  - Matter Temperature Sensor, read-only (every 10 s while valid)
//   * Spa light        - Matter Color Light   -> same "light on|off" / "lightprog N" commands as the
//                        dashboard. V0.9.4: the 2-wire lamp has 7 fixed programs, so a requested colour
//                        selects the nearest program (LIGHT_COLOURS) and the wheel snaps to it.
//                        Brightness is not adjustable; level changes are ignored.
//   * Spa wake up      - Matter On/Off Plug   -> "ready" (on) / leave Ready (off)
// No setpoint, jets, heater, Service Mode, fault reset, restart-hold or firmware control.
//
// Matter callbacks run on the Matter task. They only record the request; spaMatter::loop()
// (called from the Arduino loop task, like the web server) applies it through executeCommand()
// with the same Service/fault/settings lockout as the dashboard, then mirrors the controller's
// real state back so a refused request snaps back in Google Home.
#if SPA_MATTER && (defined(ARDUINO) || defined(SPA_MATTER_HOST_TEST))
#include <Matter.h>
#include <atomic>

namespace spaMatter {
MatterTemperatureSensor temperature;
MatterColorLight light;
MatterOnOffPlugin wake;

// -1 = no request, 0 = off, 1 = on
std::atomic<int8_t> lightRequest{-1}, wakeRequest{-1};
// 0 = no request, else the light program (1..LIGHT_PROGRAM_COUNT) nearest the requested colour
std::atomic<uint8_t> colourRequest{0}, colourPublished{1};
std::atomic<bool> colourResnap{false};
// What the controller last published; echoes of our own updates are not requests.
std::atomic<bool> lightPublished{false}, wakePublished{false};
uint32_t lastTemperatureMs = 0;
uint8_t pendingProgram = 0;   // loop task only
bool pairingLogged = false;

inline bool onLight(bool on) {
    if (on != lightPublished.load()) lightRequest.store(on ? 1 : 0);
    return true;
}
// Matter hue/saturation are 0-254. Program -> wheel colour, and requested colour -> program.
inline espHsvColor_t programHsv(uint8_t program) {
    const LightColour &c = LIGHT_COLOURS[(program >= 1 && program <= LIGHT_PROGRAM_COUNT ? program : 1) - 1];
    espHsvColor_t hsv; hsv.h = (uint8_t)((c.hue * 254UL + 180UL) / 360UL); hsv.s = c.sat; hsv.v = 254;
    return hsv;
}
inline bool onColour(espHsvColor_t hsv) {
    uint8_t program = lightProgramForColour((uint16_t)((hsv.h % 255) * 360UL / 254UL), hsv.s);
    if (program != colourPublished.load()) colourRequest.store(program);
    else {   // same programme, different shade: snap the wheel back to the real colour
        espHsvColor_t real = programHsv(program);
        if (hsv.h != real.h || hsv.s != real.s) colourResnap.store(true);
    }
    return true;
}
inline bool onWake(bool on) {
    if (on != wakePublished.load()) wakeRequest.store(on ? 1 : 0);
    return true;
}

inline String apply(const String &command, bool stop) {
    if (!stop && (serviceMode || spa.fault != FaultCode::NONE || !storageHealthy))
        return "REJECTED: Fault/Service/settings lockout";
    String result = executeCommand(command);
    liveRecord("MATTER_REQUEST", (command + " -> " + result).c_str());
    return result;
}

inline void publish() {
    lightPublished.store(spa.light);
    if (light.getOnOff() != spa.light) light.setOnOff(spa.light);
    uint8_t program = lightOn ? lightProgram : lightWantedProgram;
    if (!pendingProgram && (program != colourPublished.load() || colourResnap.exchange(false))) {
        colourPublished.store(program);   // set first: setColorHSV() calls onColour() back
        light.setColorHSV(programHsv(program));
    }
    wakePublished.store(readyMode);
    if (wake.getOnOff() != readyMode) wake.setOnOff(readyMode);
}

inline void begin() {
    // The controller is already on the home Wi-Fi with its own credentials, so pair on the
    // network: Espressif warns that BLE commissioning conflicts with sketch-held Wi-Fi
    // credentials (first Google Home attempt 2026-11-26: "Can't reach device").
    Matter.setBLECommissioningEnabled(false);
    Matter.setVendorName("Fisher");
    Matter.setProductName("Fisher Spa");
    lightPublished.store(spa.light);
    wakePublished.store(readyMode);
    colourPublished.store(lightOn ? lightProgram : lightWantedProgram);
    temperature.begin(isfinite(spa.waterTemperature) ? spa.waterTemperature : 0.0);
    light.begin(spa.light, programHsv(colourPublished.load()));
    light.onChangeOnOff(onLight);
    light.onChangeColorHSV(onColour);
    wake.begin(readyMode);
    wake.onChange(onWake);
    Matter.begin();
}

inline void loop() {
    int8_t r = lightRequest.exchange(-1);
    uint8_t p = colourRequest.exchange(0);
    if (p) pendingProgram = p;
    // Google often sends on and colour together. A colour for a light that is off becomes the
    // program it starts on (one command). A colour for a light that is on waits out the 2 s
    // input lockout instead of being refused.
    if (pendingProgram && !lightOn) {
        lightWantedProgram = pendingProgram;
        if (r == 1) { apply("light on", false); r = -1; }
        else liveRecord("MATTER_REQUEST", (String("colour -> ") + lightColourName(pendingProgram) + " (next switch-on)").c_str());
        pendingProgram = 0;
    }
    if (r >= 0 && bool(r) != spa.light) { apply(r ? "light on" : "light off", r == 0); if (r == 0) pendingProgram = 0; }
    if (pendingProgram && lightOn && !inputLocked(false)) {
        if (pendingProgram != lightProgram) apply("lightprog " + String(pendingProgram), false);
        pendingProgram = 0;
    }
    r = wakeRequest.exchange(-1);
    if (r == 1) apply("ready", false);
    else if (r == 0 && readyMode) {
        readyMode = false;   // leave Ready: normal 3 C standby band / schedule
        liveRecord("MATTER_REQUEST", "wake off -> OK: Ready ended");
    }
    publish();   // also returns a refused request to the real state

    uint32_t now = millis();
    if (now - lastTemperatureMs >= 10000UL) {
        lastTemperatureMs = now;
        if (isfinite(spa.waterTemperature)) temperature.setTemperature(spa.waterTemperature);
    }
    liveMatterPaired = Matter.isDeviceCommissioned();
    if (!pairingLogged && !Matter.isDeviceCommissioned()) {
        pairingLogged = true;
        String code = "Pair in Google Home with code " + Matter.getManualPairingCode() +
                      " or QR " + Matter.getOnboardingQRCodeUrl();
        Serial.println(code);
        liveRecord("MATTER_PAIRING", code.c_str());
    }
}
}  // namespace spaMatter
#define SPA_MATTER_ACTIVE 1
#else
#define SPA_MATTER_ACTIVE 0
#endif
