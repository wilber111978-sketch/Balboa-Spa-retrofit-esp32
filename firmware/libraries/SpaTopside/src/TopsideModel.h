#pragma once
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "TopsideConfig.h"
namespace topside {
enum class Key:uint8_t {Jets=0,Light=1,Up=2,Down=3,None=4};
enum class Gesture:uint8_t {Short,Long,Repeat};
struct Event {Key key=Key::None;Gesture gesture=Gesture::Short;};
class Buttons {
    uint8_t raw=0,stable=0;uint32_t changed=0,downAt=0,repeatAt=0,releaseAt=0;
    bool armed=false,suppressed=false,longSent=false;
public:
    bool stuck=false;
    Event poll(uint32_t now,uint8_t mask){
        mask&=15;
        if(mask!=raw){raw=mask;changed=now;}
        if(!armed){
            if(raw){releaseAt=now;if(uint32_t(now-changed)>=StuckMs)stuck=true;}
            else if(uint32_t(now-releaseAt)>=100){armed=true;suppressed=false;stuck=false;}
            return {};
        }
        if(raw&&(raw&(raw-1)))suppressed=true;
        if(uint32_t(now-changed)>=DebounceMs&&stable!=raw){
            uint8_t old=stable;stable=raw;
            if(stable){if(old)suppressed=true;downAt=repeatAt=now;longSent=false;}
            else {
                if(suppressed){armed=false;releaseAt=now;return {};}
                if(old&&!longSent){for(unsigned i=0;i<4;i++)if(old==(1<<i))return {(Key)i,Gesture::Short};}
            }
        }
        if(!stable||raw!=stable||suppressed)return {};
        if(uint32_t(now-downAt)>=StuckMs){stuck=true;suppressed=true;return {};}
        Key key=Key::None;for(unsigned i=0;i<4;i++)if(stable==(1<<i))key=(Key)i;
        if(!longSent&&uint32_t(now-downAt)>=LongPressMs){longSent=true;repeatAt=now;return {key,Gesture::Long};}
        if(longSent&&(key==Key::Up||key==Key::Down)&&uint32_t(now-repeatAt)>=RepeatMs){repeatAt=now;return {key,Gesture::Repeat};}
        return {};
    }
};
// V0.9.3 recovery controls at the tub. Two-button holds plus an on-screen confirmation, so a
// bump or a child cannot trigger them. The controller still applies every safety check.
enum class Recovery:uint8_t {None,AckRestart,ResetFault,ExitService,EmergencyStop};
struct RecoveryCombo {uint8_t mask;uint32_t holdMs;Recovery action;Key confirm;};
// Masks: Jets=1 Light=2 Up=4 Down=8. confirm==Key::None means it acts at the end of the hold.
static constexpr RecoveryCombo RecoveryCombos[]={
    {4|8,3000,Recovery::AckRestart,Key::None},      // Temp up + Temp down, 3 s
    {2|8,5000,Recovery::ResetFault,Key::Light},     // Light + Temp down, 5 s, then Light
    {1|2,5000,Recovery::ExitService,Key::Jets},     // Jets + Light, 5 s, then Jets
    {1,5000,Recovery::EmergencyStop,Key::None},     // Jets alone, 5 s: everything off
};
static constexpr uint32_t RecoveryConfirmMs=10000;
inline const char*recoveryName(Recovery r){
    return r==Recovery::AckRestart?"Acknowledge restart":r==Recovery::ResetFault?"Reset fault":
           r==Recovery::ExitService?"Exit Service Mode":r==Recovery::EmergencyStop?"Emergency stop":"";
}
class RecoveryInput {
    uint8_t held=0;uint32_t heldAt=0;bool fired=false;uint32_t pendingUntil=0;
public:
    Recovery pending=Recovery::None;Key pendingKey=Key::None;
    Recovery holding=Recovery::None;int secondsLeft=0;
    // Call every loop with the raw mask. Returns an action to run now, if any.
    Recovery poll(uint32_t now,uint8_t mask){
        mask&=15;holding=Recovery::None;secondsLeft=0;
        if(pending!=Recovery::None&&int32_t(now-pendingUntil)>=0)pending=Recovery::None;
        const RecoveryCombo*c=nullptr;for(auto&k:RecoveryCombos)if(k.mask==mask)c=&k;
        if(!c){held=0;fired=false;return Recovery::None;}
        if(held!=mask){held=mask;heldAt=now;fired=false;}
        if(fired)return Recovery::None;
        uint32_t t=now-heldAt;
        if(t<c->holdMs){holding=c->action;secondsLeft=int((c->holdMs-t+999)/1000);return Recovery::None;}
        fired=true;
        if(c->confirm==Key::None){pending=Recovery::None;return c->action;}
        pending=c->action;pendingKey=c->confirm;pendingUntil=now+RecoveryConfirmMs;return Recovery::None;
    }
    // Feed short-press events. Returns the confirmed action, or None. Any other key cancels.
    Recovery confirm(Event e){
        if(pending==Recovery::None||e.key==Key::None)return Recovery::None;
        Recovery r=pending;pending=Recovery::None;
        return e.gesture==Gesture::Short&&e.key==pendingKey?r:Recovery::None;
    }
};
enum class Screen:uint8_t {Normal,Adjust,Fault,Service,Info};
struct State {
    float water=NAN,target=38;bool valid=false,wifi=false,service=false,heatInhibited=true,logicalHeat=false,restartHold=false;
    int ageMin=-1,primingS=-1; // V0.9
    uint8_t pump=0;uint16_t fault=0;
    char faultId[32]={},description[64]={},mode[24]={},feedback[40]={};
    char prompt[40]={}; // V0.9.3 recovery countdown / confirmation, shown on every screen
};
struct View {Screen screen=Screen::Normal;char heading[32]={},large[24]={},lines[7][40]={};bool blank=false;};
inline void copy(char*dest,size_t size,const char*source){snprintf(dest,size,"%s",source);}
// Screen blackout. Goes dark after SPA_TOPSIDE_SCREEN_TIMEOUT_MS with no new button press.
// The press that wakes the screen only wakes it: the caller passes 0 to the button handler
// until every button has been released for 100 ms. Fault or Service wakes it and keeps it lit.
class Screensaver {
    bool asleep=false,swallow=false,started=false;uint8_t previous=0;uint32_t last=0,releasedAt=0;
public:
    bool dark()const{return asleep;}
    // Call every loop with the raw button mask. Returns true when this sample must be ignored.
    bool input(uint32_t now,uint8_t pressed,bool urgent,uint32_t timeoutMs=SPA_TOPSIDE_SCREEN_TIMEOUT_MS){
        pressed&=15;
        if(!started){started=true;last=now;}
        bool newPress=(pressed&~previous)!=0;previous=pressed;
        if(newPress){if(asleep){asleep=false;swallow=true;releasedAt=now;}last=now;}
        if(urgent){asleep=false;last=now;}
        if(swallow){if(pressed)releasedAt=now;else if(uint32_t(now-releasedAt)>=100)swallow=false;}
        if(!asleep&&timeoutMs&&uint32_t(now-last)>=timeoutMs)asleep=true;
        return swallow;
    }
};
class Model {
    bool adjusting=false,info=false;uint32_t interaction=0;
public:
    void noteInteraction(uint32_t now){if(adjusting||info)interaction=now;}
    void acceptedTemperature(uint32_t now){adjusting=true;info=false;interaction=now;}
    void information(uint32_t now){info=true;adjusting=false;interaction=now;}
    View view(const State&s,uint32_t now){
        View v;
        // Priority is derived entirely from the existing controller state.
        if(s.fault){
            v.screen=Screen::Fault;copy(v.heading,sizeof(v.heading),"CONTROLLER FAULT");
            snprintf(v.large,sizeof(v.large),"FAULT %u",s.fault);
            // Keep diagnostic ID within the round screen at the small font size.
            snprintf(v.lines[0],sizeof(v.lines[0]),"%.30s",s.faultId);
            size_t n=strlen(s.description);for(unsigned row=0;row<3;row++){size_t offset=row*28;if(offset<n)snprintf(v.lines[row+1],sizeof(v.lines[row+1]),"%.28s",s.description+offset);}
            copy(v.lines[4],sizeof(v.lines[4]),"HEATING INHIBITED");
            copy(v.lines[5],sizeof(v.lines[5]),"Reset: hold Light+Down 5s");
            copy(v.lines[6],sizeof(v.lines[6]),s.prompt[0]?s.prompt:s.feedback);
            return v;
        }
        if(s.service){v.screen=Screen::Service;copy(v.heading,sizeof(v.heading),"SERVICE MODE");copy(v.large,sizeof(v.large),"STOPPED");copy(v.lines[1],sizeof(v.lines[1]),"HEATING INHIBITED");copy(v.lines[3],sizeof(v.lines[3]),"Exit: hold Jets+Light 5 s");copy(v.lines[5],sizeof(v.lines[5]),s.prompt[0]?s.prompt:s.feedback);return v;}
        if(uint32_t(now-interaction)>=PreviewMs){adjusting=false;info=false;}
        if(adjusting){v.screen=Screen::Adjust;copy(v.heading,sizeof(v.heading),"TARGET TEMPERATURE");snprintf(v.large,sizeof(v.large),"%.1f C",s.target);copy(v.lines[1],sizeof(v.lines[1]),"UP / DOWN: 0.5 C");}
        else if(info){v.screen=Screen::Info;copy(v.heading,sizeof(v.heading),"CONTROLLER INFO");copy(v.large,sizeof(v.large),"V0.9.4");copy(v.lines[1],sizeof(v.lines[1]),"Heater commissioning locked");copy(v.lines[3],sizeof(v.lines[3]),"Settings: use web dashboard");}
        else {copy(v.heading,sizeof(v.heading),"SPA WATER");if(s.valid&&isfinite(s.water))snprintf(v.large,sizeof(v.large),"%.1f C",s.water);else copy(v.large,sizeof(v.large),"--.- C");snprintf(v.lines[0],sizeof(v.lines[0]),"Target %.1f C",s.target);
            // V0.9: reading age once >= 5 min; priming countdown.
            if(s.primingS>=0)snprintf(v.lines[1],sizeof(v.lines[1]),"Priming %d:%02d - heater off",s.primingS/60,s.primingS%60);
            else if(s.valid&&s.ageMin>=5)snprintf(v.lines[1],sizeof(v.lines[1]),"Updated %d min ago",s.ageMin);
            else if(!s.valid&&s.ageMin<0)copy(v.lines[1],sizeof(v.lines[1]),"Checking temperature...");
            if(s.restartHold&&s.primingS<0)copy(v.lines[3],sizeof(v.lines[3]),"Restart: hold Up+Down 3s");}
        snprintf(v.lines[2],sizeof(v.lines[2]),"Pump %s | Wi-Fi %s",s.pump==2?"HIGH":s.pump==1?"LOW":"OFF",s.wifi?"OK":"OFFLINE");
        snprintf(v.lines[4],sizeof(v.lines[4]),"Mode: %.24s",s.mode);
        copy(v.lines[5],sizeof(v.lines[5]),s.heatInhibited?(s.logicalHeat?"Demo demand / heat inhibited":"HEATING INHIBITED"):(s.logicalHeat?"Heating":"Heating idle"));
        copy(v.lines[6],sizeof(v.lines[6]),s.prompt[0]?s.prompt:s.feedback);return v;
    }
};
}
