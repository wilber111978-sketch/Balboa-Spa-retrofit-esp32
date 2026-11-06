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
enum class Screen:uint8_t {Normal,Adjust,Fault,Service,Info};
struct State {
    float water=NAN,target=38;bool valid=false,wifi=false,service=false,heatInhibited=true,logicalHeat=false;
    uint8_t pump=0;uint16_t fault=0;
    char faultId[32]={},description[64]={},mode[24]={},feedback[40]={};
};
struct View {Screen screen=Screen::Normal;char heading[32]={},large[24]={},lines[7][40]={};};
inline void copy(char*dest,size_t size,const char*source){snprintf(dest,size,"%s",source);}
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
            if(s.service)copy(v.lines[5],sizeof(v.lines[5]),"SERVICE MODE ACTIVE");
            copy(v.lines[6],sizeof(v.lines[6]),s.feedback);
            return v;
        }
        if(s.service){v.screen=Screen::Service;copy(v.heading,sizeof(v.heading),"SERVICE MODE");copy(v.large,sizeof(v.large),"STOPPED");copy(v.lines[1],sizeof(v.lines[1]),"HEATING INHIBITED");copy(v.lines[3],sizeof(v.lines[3]),"Use web confirmation to exit");copy(v.lines[5],sizeof(v.lines[5]),s.feedback);return v;}
        if(uint32_t(now-interaction)>=PreviewMs){adjusting=false;info=false;}
        if(adjusting){v.screen=Screen::Adjust;copy(v.heading,sizeof(v.heading),"TARGET TEMPERATURE");snprintf(v.large,sizeof(v.large),"%.1f C",s.target);copy(v.lines[1],sizeof(v.lines[1]),"UP / DOWN: 0.5 C");}
        else if(info){v.screen=Screen::Info;copy(v.heading,sizeof(v.heading),"CONTROLLER INFO");copy(v.large,sizeof(v.large),"V0.8");copy(v.lines[1],sizeof(v.lines[1]),"Heater commissioning locked");copy(v.lines[3],sizeof(v.lines[3]),"Settings: use web dashboard");}
        else {copy(v.heading,sizeof(v.heading),"SPA WATER");if(s.valid&&isfinite(s.water))snprintf(v.large,sizeof(v.large),"%.1f C",s.water);else copy(v.large,sizeof(v.large),"--.- C");snprintf(v.lines[0],sizeof(v.lines[0]),"Target %.1f C",s.target);}
        snprintf(v.lines[2],sizeof(v.lines[2]),"Pump %s | Wi-Fi %s",s.pump==2?"HIGH":s.pump==1?"LOW":"OFF",s.wifi?"OK":"OFFLINE");
        snprintf(v.lines[4],sizeof(v.lines[4]),"Mode: %.24s",s.mode);
        copy(v.lines[5],sizeof(v.lines[5]),s.heatInhibited?(s.logicalHeat?"Demo demand / heat inhibited":"HEATING INHIBITED"):(s.logicalHeat?"Heating":"Heating idle"));
        copy(v.lines[6],sizeof(v.lines[6]),s.feedback);return v;
    }
};
}
