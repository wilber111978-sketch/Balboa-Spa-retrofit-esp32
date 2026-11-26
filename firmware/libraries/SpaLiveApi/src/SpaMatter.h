#pragma once
// Optional Google Home / Matter bridge (off unless the sketch defines SPA_MATTER 1).
//
// Exposes exactly three things:
//   * Spa temperature  - Matter Temperature Sensor, read-only (every 10 s while valid)
//   * Spa light        - Matter On/Off Light  -> same "light on|off" command as the dashboard
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
MatterOnOffLight light;
MatterOnOffPlugin wake;

// -1 = no request, 0 = off, 1 = on
std::atomic<int8_t> lightRequest{-1}, wakeRequest{-1};
// What the controller last published; echoes of our own updates are not requests.
std::atomic<bool> lightPublished{false}, wakePublished{false};
uint32_t lastTemperatureMs = 0;
bool pairingLogged = false;

inline bool onLight(bool on) {
    if (on != lightPublished.load()) lightRequest.store(on ? 1 : 0);
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
    temperature.begin(isfinite(spa.waterTemperature) ? spa.waterTemperature : 0.0);
    light.begin(spa.light);
    light.onChange(onLight);
    wake.begin(readyMode);
    wake.onChange(onWake);
    Matter.begin();
}

inline void loop() {
    int8_t r = lightRequest.exchange(-1);
    if (r >= 0 && bool(r) != spa.light) apply(r ? "light on" : "light off", r == 0);
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
