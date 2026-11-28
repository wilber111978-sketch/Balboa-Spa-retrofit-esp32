#include "SpaTopside.h"
#define SPA_LIVE_BRIDGE 1
#include "../live-tests/api.cpp"
#include <thread>
#include <atomic>
using topside::Key;using topside::Gesture;using topside::Screen;
bool command(Key key,Gesture g=Gesture::Short){fakeMs+=300;return topsideCommand({key,g});}
void inject(uint8_t mask){topsideInputs(millis(),mask);}
void tick(uint32_t delta,uint8_t mask){fakeMs+=delta;inject(mask);}
void shortPress(uint8_t mask){tick(300,mask);tick(40,mask);tick(40,0);tick(40,0);}
void hold(uint8_t mask,uint32_t ms){for(uint32_t t=0;t<ms;t+=50){tick(50,mask);healthy();}}
void release(){for(int i=0;i<6;i++){tick(50,0);healthy();}}
struct MockCanvas {
 int x=0,y=0,size=1,writes=0;bool connected=true;unsigned delayMs=0;
 void fillScreen(int){}void setTextWrap(bool){}void setTextColor(int){}
 void setTextSize(int n){size=n;}void setCursor(int a,int b){x=a;y=b;}
 void print(const char*){if(connected)writes++;if(delayMs)std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));}
};
int main(int argc,char**argv){
 std::string test=argc>1?argv[1]:"startup";
 if(test=="watchdog-restart")fakeResetReason=ESP_RST_TASK_WDT;
 if(test=="brownout-restart")fakeResetReason=ESP_RST_BROWNOUT;
 setup();
 if(test=="watchdog-restart"||test=="brownout-restart"){
  require(serviceMode,"abnormal restart uses existing Service lockout");require(!command(Key::Jets),"panel cannot bypass restart lockout");
 }else{
  spa.fault=FaultCode::NONE;spa.lockedOut=false;serviceMode=false;storageHealthy=true;healthy();sensorsAcquired=true;primingActive=false;
  if(test=="startup"){
   loop();require(!topside::taskStarted,"host/demo requires no physical display");require(topside::pendingView.screen==Screen::Normal,"normal screen startup");
   require(strstr(topside::pendingView.lines[5],"INHIBITED"),"commissioning inhibition visible");
  }else if(test=="debounce"){
   inject(0);tick(100,0);auto t=config.setpointC;tick(300,4);tick(20,0);tick(40,0);require(config.setpointC==t,"bounce discarded");shortPress(4);require(config.setpointC==t+.5f,"40ms debounced release one increment");
  }else if(test=="light-long"){
   inject(0);tick(100,0);tick(300,2);tick(40,2);tick(799,2);require(!spa.light,"long threshold not yet reached");tick(1,2);require(topsideModel.view(topsideState(),millis()).screen==Screen::Info,"800ms long Light opens information");tick(40,0);tick(40,0);require(!spa.light,"long Light never toggles on release");shortPress(2);require(spa.light,"short Light toggles");
  }else if(test=="repeat"){
   inject(0);tick(100,0);auto t=config.setpointC;tick(300,4);tick(40,4);tick(800,4);require(config.setpointC==t+.5f,"long temperature action");tick(249,4);require(config.setpointC==t+.5f,"repeat rate bounded");tick(1,4);require(config.setpointC==t+1,"250ms repeat");tick(5000,4);require(config.setpointC==t+1.5f,"late poll no catch-up burst");
  }else if(test=="limits"){
   config.setpointC=USER_MAX_SETPOINT_C;require(!command(Key::Up),"ceiling unchanged");require(topsideModel.view(topsideState(),millis()).screen==Screen::Adjust,"limit press still shows selection");config.setpointC=20;require(!command(Key::Down),"floor unchanged");require(command(Key::Up)&&config.setpointC==20.5f,"half-degree step");
  }else if(test=="preview"){
   require(command(Key::Up),"target accepted");auto s=topsideState();require(topsideModel.view(s,millis()+4999).screen==Screen::Adjust,"preview before 5s");require(topsideModel.view(s,millis()+5000).screen==Screen::Normal,"preview ends at 5s");
   command(Key::Up);fakeMs+=4000;command(Key::Light);require(topsideModel.view(topsideState(),millis()+4999).screen==Screen::Adjust,"other interaction extends preview");
  }else if(test=="v09-priming-end"){
   primingActive=true;primingUntilMs=millis()+PRIMING_MS;auto t=config.setpointC;require(command(Key::Up),"Up during priming");require(!primingActive&&config.setpointC==t,"Up ends priming, setpoint unchanged");
  }else if(test=="v09-light-colour"){
   require(command(Key::Light),"light on");run(12000);auto p=lightProgram;require(command(Key::Light,Gesture::Long),"long Light while on");require(lightProgram==p%LIGHT_PROGRAM_COUNT+1,"next colour");require(std::string(topsideFeedback)==std::string("Light: ")+lightColourName(lightProgram),"V0.9.4: names the colour");require(topsideModel.view(topsideState(),millis()).screen!=Screen::Info,"no info screen while light on");
  }else if(test=="v09-reading-age"){
   circulatingWaterValid=true;lastCirculatingWaterC=37.2f;lastCirculatingWaterMs=millis();fakeMs+=6*60000;healthy();auto v=topsideModel.view(topsideState(),millis());require(strstr(v.large,"37.2")&&strstr(v.lines[1],"Updated 6 min ago"),"age shown after 5 min");
  }else if(test=="v09-no-reading"){
   circulatingWaterValid=false;auto v=topsideModel.view(topsideState(),millis());require(strstr(v.large,"--.-")&&strstr(v.lines[1],"Checking"),"--.- until first circulating reading");
  }else if(test=="jets-heating"){
   spa.heaterSSR=true;require(command(Key::Jets),"LOW request");run(INPUT_LOCKOUT_MS);require(command(Key::Jets),"V0.9: HIGH accepted after 2 s lockout");run(10000);require(spa.actualPump==PumpMode::HIGH_SPEED,"baseline sequences HIGH");require(!spa.heaterSSR,"HIGH excludes heater");require(command(Key::Jets),"Jets OFF request");require(!manualPumpRequest,"OFF releases manual demand only");
  }else if(test=="service"){
   executeCommand("service on");for(auto k:{Key::Jets,Key::Light,Key::Up,Key::Down})require(!command(k),"Service rejects normal requests");require(topsideModel.view(topsideState(),millis()).screen==Screen::Service,"Service display");require(command(Key::Light,Gesture::Long),"information is read-only");require(topsideModel.view(topsideState(),millis()).screen==Screen::Service,"Service overrides information");
  }else if(test=="fault-priority"){
   command(Key::Up);for(int f=1;f<=22;f++){spa.fault=(FaultCode)f;auto v=topsideModel.view(topsideState(),millis());require(v.screen==Screen::Fault,"all existing faults override preview");require(strstr(v.large,std::to_string(f).c_str()),"actual numeric fault retained");for(auto k:{Key::Jets,Key::Light,Key::Up,Key::Down})require(!command(k),"fault rejects controls");}serviceMode=true;require(topsideModel.view(topsideState(),millis()).screen==Screen::Fault,"fault wins over Service with Service label");
  }else if(test=="invalid-temperature"){
   spa.waterTemperature=NAN;auto v=topsideModel.view(topsideState(),millis());require(!strcmp(v.large,"--.- C"),"NaN never rendered as normal temperature");spa.waterTemperature=INFINITY;require(!strcmp(topsideModel.view(topsideState(),millis()).large,"--.- C"),"infinite reading invalid");healthy();fakeMs+=SENSOR_STALE_MS+1;require(!strcmp(topsideModel.view(topsideState(),millis()).large,"--.- C"),"stale measurement invalid");
  }else if(test=="stuck"){
   inject(0);tick(100,0);tick(300,1);tick(40,1);tick(10001,1);require(topsideButtons.stuck,"held input suppressed");require(!manualPumpRequest,"held Jets never starts pump");tick(40,0);tick(40,0);tick(100,0);require(!topsideButtons.stuck,"release rearms without a command");
  }else if(test=="boot-held"){
   inject(4);tick(10001,4);require(topsideButtons.stuck,"boot held detected");require(config.setpointC==38,"boot held no target change");
  }else if(test=="wifi-loss"){
   WiFi.state=0;require(command(Key::Jets),"local operation independent of Wi-Fi");auto v=topsideModel.view(topsideState(),millis());require(strstr(v.lines[2],"OFFLINE"),"offline indicator");raiseFault(FaultCode::OVERTEMP,true);require(!command(Key::Jets),"offline cannot bypass fault");
  }else if(test=="rapid-conflicts"){
   require(command(Key::Up),"initial up");auto t=config.setpointC;require(!topsideCommand({Key::Down,Gesture::Short})&&config.setpointC==t,"rapid conflict rejected");expectCode(liveHTTP("/api/target","{\"target_c\":37}",true,true),429);fakeMs+=300;expectCode(liveHTTP("/api/target","{\"target_c\":37}",true,true),200);require(!topsideCommand({Key::Up,Gesture::Short}),"API and panel share limiter");
  }else if(test=="simultaneous"){
   inject(0);tick(100,0);shortPress(12);require(config.setpointC==38,"UP+DOWN discarded");
  }else if(test=="display-disconnect"){
   MockCanvas c;c.connected=false;auto v=topsideModel.view(topsideState(),millis());for(int y=0;y<240;y+=8)topside::paintStrip(c,v,y);require(c.writes==0,"disconnected sink ignores drawing");require(command(Key::Light),"display loss does not block controller");c.connected=true;topside::paintStrip(c,v,56);require(c.writes>0,"renderer can resume after sink reconnect");
  }else if(test=="render-delay"){
   auto v=topsideModel.view(topsideState(),millis());std::atomic<bool> started=false,finished=false;
   std::thread display([&]{MockCanvas c;c.delayMs=300;started=true;topside::paintStrip(c,v,56);finished=true;});
   while(!started)std::this_thread::yield();
   raiseFault(FaultCode::OVERTEMP,true);for(int i=0;i<50;i++)step(1);
   require(spa.fault==FaultCode::OVERTEMP&&!pins[2]&&!pins[42],"fault processing while renderer delayed");require(!finished,"controller progressed before slow rendering ended");display.join();
  }else if(test=="persist-failure"){
   Preferences::failWrite=true;require(!command(Key::Up),"failed storage rejected");require(!storageHealthy&&!command(Key::Jets),"settings failure lockout retained");
  }else if(test=="rec-ack-restart"){
   inject(0);release();heaterRestartHold=true;hold(12,2500);require(heaterRestartHold,"not before 3 s");
   require(strstr(topsideState().prompt,"in 1 s"),"countdown shown");release();require(heaterRestartHold,"early release does nothing");
   hold(12,3100);release();require(SPA_BENCH_DEMO?heaterRestartHold:!heaterRestartHold,SPA_BENCH_DEMO?"Demo never acknowledges (same as web)":"Up+Down 3 s acknowledges restart");
  }else if(test=="rec-reset-fault"){
   inject(0);release();raiseFault(FaultCode::TEMP_DISAGREE,true);require(spa.fault!=FaultCode::NONE,"fault set");
   hold(10,5100);release();require(spa.fault!=FaultCode::NONE,"hold alone only arms");require(strstr(topsideState().prompt,"Press LIGHT"),"asks for confirmation");
   shortPress(2);require(spa.fault==FaultCode::NONE,"Light confirms reset");
  }else if(test=="rec-reset-cancel"){
   inject(0);release();raiseFault(FaultCode::TEMP_DISAGREE,true);hold(10,5100);release();shortPress(1);
   require(spa.fault!=FaultCode::NONE,"another key cancels");require(spa.requestedPump==PumpMode::OFF,"cancel press is not a Jets command");
  }else if(test=="rec-reset-unsafe"){
   inject(0);release();raiseFault(FaultCode::TEMP_DISAGREE,true);ads.raw[0]=0;ads.raw[1]=0;hold(10,5100);release();shortPress(2);
   require(SPA_BENCH_DEMO||spa.fault!=FaultCode::NONE,"reset refused while cause present (Main reads the real ADC; Demo injects temperatures)");
  }else if(test=="rec-exit-service"){
   inject(0);release();executeCommand("service on");require(serviceMode,"service on");
   hold(3,5100);release();require(serviceMode,"hold alone only arms");shortPress(1);require(!serviceMode,"Jets confirms exit");
  }else if(test=="rec-exit-timeout"){
   inject(0);release();executeCommand("service on");hold(3,5100);release();tick(11000,0);shortPress(1);require(serviceMode,"confirmation expires after 10 s");
  }else if(test=="rec-exit-with-fault"){
   inject(0);release();executeCommand("service on");raiseFault(FaultCode::TEMP_DISAGREE,true);hold(3,5100);release();shortPress(1);require(serviceMode,"cannot exit Service with a fault");
  }else if(test=="rec-estop"){
   inject(0);release();require(!serviceMode,"running");hold(1,4900);require(!serviceMode,"not before 5 s");hold(1,200);release();require(serviceMode,"Jets 5 s = emergency stop");
  }else if(test=="rec-no-bump"){
   inject(0);release();executeCommand("service on");hold(3,4000);release();shortPress(1);require(serviceMode,"4 s bump does not arm");
   executeCommand("service off confirm");release();hold(12,1000);release();shortPress(1);require(spa.requestedPump==PumpMode::LOW_SPEED,"normal Jets still works after an aborted combo");
  }else if(test=="schedule-unchanged"){
   auto mode=heatMode,start=solarStart,end=solarEnd;command(Key::Up);command(Key::Down);command(Key::Light);command(Key::Jets);require(heatMode==mode&&solarStart==start&&solarEnd==end,"UI never changes heating mode or schedule");
  }else require(false,"unknown scenario");
 }
 require(!pins[1]&&!pins[41],"CH1 and CH3 remain physically OFF");require(!(pins[2]&&pins[42]),"pump outputs exclusive");
 std::cout<<"PASS "<<test<<"\n";
}
