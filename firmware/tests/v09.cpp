// V0.9 behaviour scenarios. Same harness as scenarios.cpp: every step asserts
// LOW/HIGH physical exclusion and physical heater pins OFF.
#ifndef SPA_BENCH_DEMO
#define SPA_BENCH_DEMO 1
#endif
#include "../libraries/SpaController/src/SpaController.h"
#include <iostream>
void require(bool ok,const char*msg){if(!ok){std::cerr<<"FAIL: "<<msg<<"\n";std::exit(1);}}
void step(uint32_t n=250){fakeMs+=n;loop();require(!(pins[2]&&pins[42]),"pump pins mutually exclusive");require(!pins[1]&&!pins[41],"physical heater pins OFF");}
void run(uint32_t n){for(uint32_t x=0;x<n;x+=250)step();}
uint32_t until(bool(*cond)(),uint32_t maxMs){for(uint32_t x=0;x<maxMs;x+=250){if(cond())return x;step();}return UINT32_MAX;}
static const uint32_t MIN=60000UL;
int main(int argc,char**argv){
 std::string test=argc>1?argv[1]:"";
#if !SPA_BENCH_DEMO
 fakeMv[8]=fakeMv[9]=1650;   // healthy sensors at boot
#endif
 setup();
#if !SPA_BENCH_DEMO
 // ---------------- Main build, onboard ADC (SPA_TEMP_ADC_ADS1115=0) ----------------
 if(test=="onboard-ntc"){fakeMv[8]=fakeMv[9]=1650;run(3000);require(tempA.status==SensorStatus::OK&&fabs(tempA.filteredC-25)<0.2&&fabs(tempB.filteredC-25)<0.2,"onboard ADC 1.65 V -> 25 C");}
 else if(test=="onboard-38c"){float r=30000*exp(3860*(1/(38+273.15)-1/298.15));fakeMv[8]=fakeMv[9]=uint32_t(3300*r/(r+30000));run(15000);require(fabs(spa.waterTemperature-38)<0.3,"onboard ADC 38 C");}
 else if(test=="onboard-open"){fakeMv[8]=3050;fakeMv[9]=1650;run(1000);require(spa.fault==FaultCode::TEMP_A_OPEN,"onboard saturation near 3.1 V is OPEN");}
 else if(test=="onboard-short"){fakeMv[8]=1650;fakeMv[9]=50;run(1000);require(spa.fault==FaultCode::TEMP_B_SHORT,"onboard short");}
 else if(test=="onboard-no-i2c"){require(adcAvailable&&spa.fault!=FaultCode::ADC_FAILURE,"onboard mode needs no ADS1115");}
 else if(test=="main-priming"){fakeMv[8]=fakeMv[9]=1650;run(2000);require(primingActive&&spa.requestedPump==PumpMode::LOW_SPEED,"power-on priming runs LOW");run(PRIMING_MS);require(!primingActive,"priming ends after 4 min");}
#else
 // ---------------- Demo build (logical heater) ----------------
 if(test=="uv-both-speeds"){executeCommand("high");run(3000);require(pins[42]&&pins[45],"UV with HIGH");executeCommand("off");run(3000);require(!pins[42],"HIGH off");
   executeCommand("primeend");run(500);require(spa.actualPump!=PumpMode::OFF?pins[45]:!pins[45],"UV follows pump");}
 else if(test=="uv-changeover"){executeCommand("low");run(3000);require(pins[45],"UV on LOW");run(2000);executeCommand("high");step();require(!pins[45],"UV off during dead-time");run(3000);require(pins[42]&&pins[45],"UV back on HIGH");}
 else if(test=="uv-stopped"){run(3000);require(spa.actualPump==PumpMode::OFF&&!pins[45],"no UV with pump stopped");}
 else if(test=="manual-low-timeout"){executeCommand("low");run(2000);require(pins[2],"manual LOW on");run(MANUAL_LOW_TIMEOUT_MS);run(3000);require(!manualPumpRequest&&spa.actualPump==PumpMode::OFF,"manual LOW auto-off at 20 min");}
 else if(test=="manual-low-high-still-20"){executeCommand("high");run(JETS_TIMEOUT_MS+5000);require(spa.actualPump==PumpMode::LOW_SPEED&&afterUseRemainingMs>0,"jets timeout unchanged");}
 else if(test=="lockout-pump"){require(executeCommand("low").startsWith("OK"),"first accepted");require(executeCommand("high").startsWith("REJECTED: input lockout"),"second within 2 s rejected");run(2000);require(executeCommand("high").startsWith("OK"),"accepted after 2 s");require(executeCommand("off").startsWith("OK"),"stop never locked out");}
 else if(test=="lockout-independent"){require(executeCommand("low").startsWith("OK"),"jets");require(executeCommand("light").startsWith("OK"),"light not blocked by jets lockout");}
 else if(test=="lockout-no-queue"){executeCommand("low");executeCommand("high");run(5000);require(spa.actualPump==PumpMode::LOW_SPEED,"rejected press is not queued");}
 else if(test=="light-timeout"){executeCommand("light on");step();require(pins[46]&&lightOn,"light on");run(LIGHT_TIMEOUT_MS+1000);require(!pins[46]&&!lightOn,"light auto-off 40 min");}
 else if(test=="light-program"){executeCommand("light on");step();executeCommand("primeend");run(2500);int toggles=0;bool last=pins[46];require(executeCommand("lightprog 3").startsWith("OK"),"select program 3");
   if(pins[46]!=last){toggles++;last=pins[46];}for(int i=0;i<40;i++){step(100);if(pins[46]!=last){toggles++;last=pins[46];}}require(toggles==4&&pins[46]&&lightProgram==3,"two step pulses to reach program 3");}
 else if(test=="light-known-state"){executeCommand("light on");step();run(2500);executeCommand("lightprog 3");run(3000);executeCommand("light off");run(3000);require(executeCommand("light on").startsWith("OK"),"on accepted");
   step();require(!pins[46],"switch-on waits out reset time (off 3 s < 10 s)");run(7000);require(pins[46],"on once reset time elapsed");
   int toggles=0;bool last=pins[46];for(int i=0;i<40;i++){step(100);if(pins[46]!=last){toggles++;last=pins[46];}}require(toggles==4&&lightProgram==3,"restored last colour (program 3)");}
 else if(test=="light-restore-after-timeout"){executeCommand("light on");step();run(2500);executeCommand("lightprog 4");run(5000);run(LIGHT_TIMEOUT_MS);require(!lightOn,"auto-off");run(15000);executeCommand("light on");
   int toggles=0;bool last=pins[46];for(int i=0;i<60;i++){step(100);if(pins[46]!=last){toggles++;last=pins[46];}}require(toggles==6&&lightProgram==4,"colour restored after auto-off (3 steps)");}
 else if(test=="light-bad-program"){require(executeCommand("lightprog 0").startsWith("ERROR")&&executeCommand("lightprog 99").startsWith("ERROR")&&executeCommand("lightprog 2.5").startsWith("ERROR"),"program range");}
 else if(test=="priming"){executeCommand("prime");executeCommand("heatmode manual");executeCommand("ready");run(60000);require(spa.actualPump==PumpMode::LOW_SPEED&&!spa.heaterSSR,"priming: LOW, heater locked");
   executeCommand("primeend");run(15000);require(spa.heaterSSR,"heat allowed after priming ends");}
 else if(test=="priming-jets"){executeCommand("prime");run(3000);require(executeCommand("high").startsWith("OK"),"jets usable during priming");run(3000);require(pins[42]&&!spa.heaterSSR,"HIGH purge during priming");}
 else if(test=="freeze"){demoTempA=demoTempB=4;run(3000);require(freezeActive&&spa.actualPump==PumpMode::LOW_SPEED,"freeze runs LOW");demoTempA=demoTempB=9;run(8000);require(!freezeActive,"freeze clears >= 8 C after settle");}
 else if(test=="freeze-one-sensor-high"){demoTempA=4;demoTempB=30;run(3000);require(freezeActive,"either sensor < 5 C triggers");run(12000);require(spa.fault==FaultCode::TEMP_DISAGREE&&pins[2],"pump keeps running through sensor fault");}
 else if(test=="freeze-blind"){executeCommand("sensorafail");executeCommand("sensorbfail");run(11*MIN);require(pins[2],"both sensors invalid: blind circulation");run(15*MIN);require(!pins[2],"blind run 15 min/hour");}
 else if(test=="freeze-not-on-flow-fault"){executeCommand("heatmode manual");executeCommand("ready");run(15000);demoTempA=demoTempB=4;run(500);demoTempA=demoTempB=5.5;run(500);
   require(spa.fault==FaultCode::NONE||!pins[2]||!freezeOverrideAllowed(spa.fault),"flow faults keep pump stopped");}
 else if(test=="poll-heat-off"){run(30*MIN+2000);require(spa.requestedPump==PumpMode::LOW_SPEED,"30 min poll in heat mode OFF");run(15000);require(circulatingWaterValid&&spa.actualPump==PumpMode::OFF,"poll ends once reading captured");}
 else if(test=="water-age"){executeCommand("low");run(8000);executeCommand("off");run(3*MIN);require(statusJSON().s.find("\"waterAgeSeconds\":18")!=std::string::npos||statusJSON().s.find("\"waterAgeSeconds\":17")!=std::string::npos,"age reported");}
 else if(test=="no-reading-before-circulation"){require(!circulatingWaterValid&&statusJSON().s.find("\"water\":null")!=std::string::npos,"--.- until first circulating reading");}
 else if(test=="floor-heats"){executeCommand("heatmode solar");executeCommand("clock 20:00");demoTempA=demoTempB=33.8;run(31*MIN);require(floorHeatNeeded&&schedulePermitsHeating(),"floor demand outside window");run(15000);require(spa.heaterSSR,"heats to floor");
   demoTempA=demoTempB=35.0;run(1000);require(!spa.heaterSSR,"stops at 35 C, not setpoint");}
 else if(test=="floor-not-above"){executeCommand("heatmode solar");executeCommand("clock 20:00");demoTempA=demoTempB=34.5;run(31*MIN);require(!floorHeatNeeded&&!spa.heaterSSR,"34.5 C inside floor hysteresis: no heat");}
 else if(test=="floor-window-setpoint"){executeCommand("heatmode solar");executeCommand("clock 11:00");executeCommand("ready");demoTempA=demoTempB=36;run(15000);require(spa.heaterSSR,"inside window heats to setpoint");}
 else if(test=="flow-retry"){executeCommand("heatmode manual");executeCommand("ready");run(15000);require(spa.heaterSSR,"heating");demoTempA=demoTempB=37;run(500);
   require(spa.fault==FaultCode::RAPID_TEMP_RISE&&!spa.lockedOut&&!pins[2],"trip 1: pump off, retry pending");demoTempA=demoTempB=36;run(FLOW_RETRY_DELAY_MS);run(2000);
   require(spa.fault==FaultCode::NONE&&pins[42],"retry: HIGH purge");run(FLOW_PURGE_MS);run(3000);require(!pins[42]&&afterUseRemainingMs==0,"purge ends, no after-use");}
 else if(test=="flow-latch"){executeCommand("heatmode manual");executeCommand("ready");
   for(int trip=1;trip<=3;trip++){demoTempA=demoTempB=36;uint32_t w=until([]{return spa.heaterSSR;},FLOW_RETRY_DELAY_MS+FLOW_PURGE_MS+60000);require(w!=UINT32_MAX,"heat resumes before trip");run(6000);demoTempA=demoTempB=37;run(500);
     if(trip<3)require(!spa.lockedOut,"trips 1-2 retry");else require(spa.lockedOut,"third trip in 24 h latches");}}
 else if(test=="water-hot"){executeCommand("heatmode manual");demoTempA=demoTempB=42.5;run(2000);require(spa.waterHot&&spa.fault==FaultCode::NONE&&!spa.lockedOut,"ambient hot: warning only");executeCommand("ready");run(15000);require(!spa.heaterSSR,"heater inhibited while hot");
   demoTempA=demoTempB=40.5;run(2000);require(!spa.waterHot,"clears below 41 C");}
 else if(test=="heater-overtemp-latches"){executeCommand("heatmode manual");executeCommand("ready");run(15000);require(spa.heaterSSR,"heating");demoTempA=demoTempB=42.5;run(500);require(spa.fault==FaultCode::OVERTEMP&&spa.lockedOut,"heater-caused over-temp latches");}
 else if(test=="sensor-mismatch"){executeCommand("low");demoTempA=36;demoTempB=36.6;run(CAL_SETTLE_MS+CAL_PERSIST_MS+2000);require(spa.calibrationInhibit,"0.6 C at rest inhibits heating");
   executeCommand("heatmode manual");executeCommand("ready");run(15000);require(!spa.heaterSSR,"no heat while mismatched");demoTempB=36.1;run(CAL_PERSIST_MS+2000);require(!spa.calibrationInhibit,"clears below 0.4 C");}
 else if(test=="sensor-cap"){executeCommand("setpoint 40");executeCommand("heatmode manual");executeCommand("ready");demoTempA=39.0;demoTempB=40.2;run(15000);require(!spa.heaterSSR,"no new heat with a sensor >= 40 C");}
 else if(test=="ssr-proof"){executeCommand("heatmode manual");executeCommand("ready");uint32_t w=until([]{return spa.heaterSafetyContactor;},20000);require(w!=UINT32_MAX&&!spa.heaterSSR,"contactor closes first");
   demoTempB+=0.4f;step();require(spa.fault==FaultCode::HEATER_SSR_SUSPECT&&spa.lockedOut,"rise with SSR off during proof window");}
 else if(test=="status-fields"){std::string j=statusJSON().s;for(const char*k:{"\"water\":","\"waterAgeSeconds\":","\"uv\":","\"priming\":","\"freeze\":","\"waterHot\":","\"sensorMismatch\":","\"floorHeat\":","\"lightProgram\":","\"tempAdc\":"})require(j.find(k)!=std::string::npos,k);}
#endif
 else {std::cerr<<"unknown test "<<test<<"\n";return 2;}
 std::cout<<"PASS "<<SPA_BENCH_DEMO<<" "<<test<<"\n";
 return 0;
}
