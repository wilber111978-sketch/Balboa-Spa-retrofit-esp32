#pragma once
// Host stand-in for the Arduino-ESP32 Matter library (test only).
// setOnOff() echoes into the change callback, the worst case for feedback loops.
#include <functional>
struct MatterOnOffStub {
    bool state=false; int begun=0; std::function<bool(bool)> cb;
    bool begin(bool s=false){state=s;begun++;return true;}
    void onChange(std::function<bool(bool)> f){cb=f;}
    bool getOnOff(){return state;}
    bool setOnOff(bool s){if(s==state)return true;state=s;if(cb)cb(s);return true;}
    bool googleSets(bool s){bool ok=cb?cb(s):true;if(ok)state=s;return ok;}   // controller-side write
};
struct MatterOnOffLight: MatterOnOffStub {};
struct MatterOnOffPlugin: MatterOnOffStub {};
struct MatterTemperatureSensor { double value=-1000; bool begin(double t=0){value=t;return true;} bool setTemperature(double t){value=t;return true;} };
struct MatterStub { int begins=0; bool commissioned=false;
    void begin(){begins++;} bool isDeviceCommissioned(){return commissioned;}
    String getManualPairingCode(){return String("34970112332");} String getOnboardingQRCodeUrl(){return String("https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:TEST");} };
inline MatterStub Matter;
