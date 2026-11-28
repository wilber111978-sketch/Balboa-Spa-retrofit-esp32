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
struct espHsvColor_t { uint16_t h=0; uint8_t s=0; uint8_t v=0; };
// Colour light: hue/sat on the Matter 0-254 scale, like the Arduino-ESP32 library.
// setColorHSV() echoes into the colour callback (the library calls it back too).
struct MatterColorLight: MatterOnOffStub {
    espHsvColor_t hsv; int colourSets=0; std::function<bool(espHsvColor_t)> colourCb;
    bool begin(bool s=false,espHsvColor_t c={0,254,31}){state=s;hsv=c;begun++;return true;}
    void onChangeOnOff(std::function<bool(bool)> f){cb=f;}
    void onChangeColorHSV(std::function<bool(espHsvColor_t)> f){colourCb=f;}
    espHsvColor_t getColorHSV(){return hsv;}
    bool setColorHSV(espHsvColor_t c){hsv=c;colourSets++;if(colourCb)colourCb(c);return true;}
    bool googleColour(uint16_t h254,uint8_t s){espHsvColor_t c;c.h=h254;c.s=s;c.v=254;bool ok=colourCb?colourCb(c):true;if(ok)hsv=c;return ok;}
};
struct MatterOnOffPlugin: MatterOnOffStub {};
struct MatterTemperatureSensor { double value=-1000; bool begin(double t=0){value=t;return true;} bool setTemperature(double t){value=t;return true;} };
struct MatterStub { uint32_t beginDelayMs=0; int begins=0; bool commissioned=false; bool ble=true; int namesSet=0;
    void begin(){begins++;fakeMs+=beginDelayMs;}
    bool setBLECommissioningEnabled(bool e){if(begins)return false;ble=e;return true;}
    bool setVendorName(const char*){namesSet++;return !begins;} bool setProductName(const char*){namesSet++;return !begins;} bool isDeviceCommissioned(){return commissioned;}
    String getManualPairingCode(){return String("34970112332");} String getOnboardingQRCodeUrl(){return String("https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:TEST");} };
inline MatterStub Matter;
