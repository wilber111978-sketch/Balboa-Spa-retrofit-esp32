#pragma once
#define SPA_LIVE_EXTERNAL_ENTRYPOINTS 1
#define SPA_LIVE_VERSION "0.9.0-topside"
#define SPA_LIVE_BUILD "gc9a01-ui-2"
#include <SpaLiveApi.h>
#include "TopsideModel.h"
#include "TopsideDisplay.h"
static topside::Buttons topsideButtons;
static topside::Model topsideModel;
static char topsideFeedback[40]={};static uint32_t topsideFeedbackAt=0;
static void topsideMessage(const char*text){topside::copy(topsideFeedback,sizeof(topsideFeedback),text);topsideFeedbackAt=millis();}
static bool topsideCommand(topside::Event e){
    using topside::Key;using topside::Gesture;
    if(e.key==Key::None)return false;
    topsideModel.noteInteraction(millis());
    // V0.9: long Light steps colour while the light is ON; otherwise opens information.
    if(e.key==Key::Light&&e.gesture==Gesture::Long){
        if(!lightOn||serviceMode||spa.fault!=FaultCode::NONE){topsideModel.information(millis());return true;}
        String r=executeCommand("lightprog "+String(lightProgram%LIGHT_PROGRAM_COUNT+1));
        topsideMessage(r.startsWith("OK")?"Next light colour":"Rejected: wait 2 s");return r.startsWith("OK");
    }
    // V0.9: Temp +/- during priming ends priming (as on the OEM panel).
    if((e.key==Key::Up||e.key==Key::Down)&&primingActive){executeCommand("primeend");topsideMessage("Priming ended");return true;}
    if((e.key==Key::Jets||e.key==Key::Light)&&e.gesture!=Gesture::Short)return false;
    // Same normal-command lockouts as the live API, stricter than raw serial.
    if(serviceMode||spa.fault!=FaultCode::NONE||!storageHealthy){topsideMessage("Rejected: fault / Service / settings");return false;}
    String command;
    if(e.key==Key::Jets)command=spa.requestedPump==PumpMode::OFF?"low":spa.requestedPump==PumpMode::LOW_SPEED?"high":"off";
    else if(e.key==Key::Light)command="light";
    else {
        topsideModel.acceptedTemperature(millis()); // show current selection even at a limit
        float next=config.setpointC+(e.key==Key::Up?0.5f:-0.5f);
        if(!isfinite(next)||next<20||next>USER_MAX_SETPOINT_C){topsideMessage("Target limit reached");return false;}
        command="setpoint "+String(next,1);
    }
    bool stop=command=="off"||(e.key==Key::Light&&spa.light);
    if(!stop&&liveCommandSeen&&uint32_t(millis()-liveLastCommandMs)<250){topsideMessage("Rejected: requests too fast");return false;}
    String result=executeCommand(command);
    if(!result.startsWith("OK")){topsideMessage(result.startsWith("REJECTED: input lockout")?"Wait 2 s between presses":"Rejected: controller / storage");return false;}
    if(!stop){liveCommandSeen=true;liveLastCommandMs=millis();}
    if(e.key==Key::Up||e.key==Key::Down)topsideModel.acceptedTemperature(millis());
    topsideMessage("Request accepted");liveRecord("TOPSIDE_REQUEST",command.c_str());return true;
}
inline void topsideInputs(uint32_t now,uint8_t pressed){
    auto event=topsideButtons.poll(now,pressed);
    if(topsideButtons.stuck)topsideMessage("Button stuck: release all buttons");
    else topsideCommand(event);
}
static topside::State topsideState(){
    // V0.9: show the last reading taken with water circulating, never stagnant probes.
    topside::State s;s.water=lastCirculatingWaterC;s.target=config.setpointC;
    s.valid=sensorsAcquired&&circulatingWaterValid&&tempA.status==SensorStatus::OK&&tempB.status==SensorStatus::OK&&
        uint32_t(millis()-tempA.lastUpdateMs)<=SENSOR_STALE_MS&&uint32_t(millis()-tempB.lastUpdateMs)<=SENSOR_STALE_MS&&isfinite(lastCirculatingWaterC);s.wifi=WiFi.status()==WL_CONNECTED;
    s.ageMin=circulatingWaterValid?int((millis()-lastCirculatingWaterMs)/60000UL):-1;
    s.primingS=primingActive?int((primingUntilMs-millis())/1000UL):-1;
    s.service=serviceMode;s.fault=(uint16_t)spa.fault;s.logicalHeat=spa.heaterSSR;
    s.pump=spa.actualPump==PumpMode::HIGH_SPEED?2:spa.actualPump==PumpMode::LOW_SPEED?1:0;
    // Physical CH1/CH3 remain inhibited in BOTH builds. Never decide heat safety here.
    s.heatInhibited=true;
    auto f=liveFaultInfo(spa.fault);topside::copy(s.faultId,sizeof(s.faultId),f.id);topside::copy(s.description,sizeof(s.description),f.title);
    topside::copy(s.mode,sizeof(s.mode),heatMode==1?"Solar Priority":heatMode==2?"Manual request":"Heating OFF");
    if(uint32_t(millis()-topsideFeedbackAt)<3000)topside::copy(s.feedback,sizeof(s.feedback),topsideFeedback);
    return s;
}
#if SPA_BENCH_DEMO
// Test/bench C++ hook only: no HTTP or serial injection route, absent from Main.
inline void topsideSimulateButtons(uint8_t pressed){topsideInputs(millis(),pressed);}
#endif
void setup(){spaLiveSetup();topside::displayBegin();}
void loop(){
    spaLiveLoop(); // controller fault evaluation always precedes UI work
#if defined(ARDUINO) && SPA_TOPSIDE_HARDWARE
    uint8_t pressed=0;for(unsigned i=0;i<4;i++)if(digitalRead(topside::ButtonPins[i])==LOW)pressed|=1<<i;
    topsideInputs(millis(),pressed);
#endif
    // Only small value copies here; no display.begin(), SPI, waits or rendering.
    topside::publish(topsideModel.view(topsideState(),millis()));
}
