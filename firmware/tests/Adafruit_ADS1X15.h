#pragma once
#define GAIN_ONE 1
#define ADS1X15_REG_CONFIG_MUX_SINGLE_0 0x4000
#define ADS1X15_REG_CONFIG_MUX_SINGLE_1 0x5000
class Adafruit_ADS1115{public:bool connected=true,stuck=false;int channel=0;int16_t raw[2]={13200,13200};bool begin(){return connected;}void setGain(int){}void startADCReading(uint16_t mux,bool){channel=mux==0x5000?1:0;}bool conversionComplete(){return !stuck;}int16_t getLastConversionResults(){return raw[channel];}float computeVolts(int16_t x){return x*0.000125f;}};
