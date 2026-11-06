#pragma once
#include "TopsideModel.h"
namespace topside {
// Same render commands for the device and host framebuffer/mock.
// A canvas implementation supplies fillScreen/setCursor/setText.../print.
template<class Canvas> void paintStrip(Canvas& c,const View&v,int y){
    c.fillScreen(0);c.setTextWrap(false);
    uint16_t accent=v.screen==Screen::Fault?0xf800:v.screen==Screen::Service?0xfd20:0x07ff;
    auto text=[&](const char*value,int top,unsigned size,uint16_t color){
        if(top+int(8*size)<=y||top>=y+8)return;
        // Keep text inside the round 240x240 active area, including bottom rows.
        unsigned maxChars=top>=200?25:top>=180?30:38;
        char clipped[40];snprintf(clipped,sizeof(clipped),"%.*s",(int)maxChars,value);
        int width=(int)strlen(clipped)*6*size;
        c.setTextColor(color);c.setTextSize(size);c.setCursor((240-width)/2,top-y);c.print(clipped);
    };
    text(v.heading,26,1,accent);text(v.large,58,4,0xffff);
    for(unsigned row=0;row<7;row++)text(v.lines[row],108+row*16,1,row==5?accent:0xffff);
}
}
#if defined(ARDUINO) && SPA_TOPSIDE_HARDWARE
#include <Adafruit_GC9A01A.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace topside {
static portMUX_TYPE viewMux=portMUX_INITIALIZER_UNLOCKED;
static View pendingView;static bool hasView=false;
static uint32_t urgentGeneration=0;
static bool taskStarted=false;
inline void publish(const View&v){
    portENTER_CRITICAL(&viewMux);
    if(hasView&&v.screen!=pendingView.screen)++urgentGeneration;
    pendingView=v;hasView=true;
    portEXIT_CRITICAL(&viewMux);
}
inline View readView(uint32_t&generation){
    View v;portENTER_CRITICAL(&viewMux);v=pendingView;generation=urgentGeneration;portEXIT_CRITICAL(&viewMux);return v;
}
static void displayTask(void*){
    // All SPI allocation, hardware reset and library delays belong to this task.
    SPIClass spi(FSPI);spi.begin(Sclk,-1,Mosi,Cs);
    Adafruit_GC9A01A lcd(&spi,Dc,Cs,Reset);
    GFXcanvas16 strip(240,8); // 3840 bytes; allocated once, no full framebuffer
    if(!strip.getBuffer()){vTaskDelete(nullptr);return;}
    lcd.begin(24000000);lcd.setRotation(0);
#if SPA_TOPSIDE_BACKLIGHT_VERIFIED
    digitalWrite(Backlight,HIGH); // external driver ENABLE only; never LED current
#endif
    View last={};bool first=true;
    for(;;){
        uint32_t generation;View v=readView(generation);
        if(first||memcmp(&last,&v,sizeof(v))){
            for(int y=0;y<240;y+=8){
                uint32_t currentGeneration;View current=readView(currentGeneration);
                if(currentGeneration!=generation){v=current;generation=currentGeneration;y=0;}
                paintStrip(strip,v,y);
                lcd.startWrite();lcd.setAddrWindow(0,y,240,8);lcd.writePixels(strip.getBuffer(),240*8);lcd.endWrite();
                vTaskDelay(1); // yield between bounded strips; no core/controller locks
            }
            last=v;first=false;
        }
        vTaskDelay(pdMS_TO_TICKS(RefreshMs));
    }
}
inline void displayBegin(){
    // CS deasserted, backlight driver off before creating the worker.
    digitalWrite(Cs,HIGH);pinMode(Cs,OUTPUT);digitalWrite(Backlight,LOW);pinMode(Backlight,OUTPUT);
    for(int pin:ButtonPins)pinMode(pin,INPUT_PULLUP);
    taskStarted=xTaskCreatePinnedToCore(displayTask,"SpaDisplay",6144,nullptr,1,nullptr,0)==pdPASS;
    // Failure is a peripheral issue, never a new controller fault or reset request.
}
}
#else
namespace topside {
static View pendingView;static bool taskStarted=false;
inline void displayBegin(){taskStarted=false;}
inline void publish(const View&v){pendingView=v;}
}
#endif
