// Google Home / Matter bridge logic against the real controller (host stub for Matter).
#define main frozen_regression_main
#include "../tests/scenarios.cpp"
#undef main
using spaMatter::light; using spaMatter::wake; using spaMatter::temperature;
int main(int argc,char **argv){std::string test=argc>1?argv[1]:"begin";if(test=="slow-begin"){Matter.beginDelayMs=3000;}fakeMv[8]=fakeMv[9]=1650;setup();primingActive=false;run(3000);require(spa.fault==FaultCode::NONE,"healthy sensors");
 require(Matter.begins==1&&light.begun==1&&wake.begun==1,"bridge started once after the controller");
 require(!Matter.ble&&Matter.namesSet==2,"on-network pairing (BLE commissioning off) and names set before Matter.begin");
 require(WiFi.ipv6,"station IPv6 enabled for Matter");
 if(test=="begin"){step();require(!light.getOnOff()&&!wake.getOnOff(),"starts mirroring real state");}
 else if(test=="temperature"){run(11000);require(isfinite(spa.waterTemperature),"valid reading");require(fabs(temperature.value-spa.waterTemperature)<0.01,"publishes controller water temperature");}
 else if(test=="light"){run(3000);light.googleSets(true);step();require(spa.light&&pins[46],"Google light on uses the dashboard command");run(3000);light.googleSets(false);step();require(!spa.light&&!pins[46],"Google light off");}
 else if(test=="wake"){wake.googleSets(true);step();require(readyMode,"wake up = Ready");uint32_t t=lastInteractionMs;run(5000);require(lastInteractionMs==t,"our own state echo does not re-extend Ready");wake.googleSets(false);step();require(!readyMode&&!wake.getOnOff(),"wake off leaves Ready");}
 else if(test=="slow-begin"){require(spa.fault==FaultCode::NONE,"slow Matter.begin does not latch SENSOR_STALE");}
 else if(test=="ready-timeout"){wake.googleSets(true);step();require(wake.getOnOff(),"on");fakeMs+=READY_TIMEOUT_MS;step();step();require(!readyMode&&!wake.getOnOff(),"Ready timeout shows as off in Google Home");}
 else if(test=="service-lockout"){setServiceMode(true);run(3000);light.googleSets(true);wake.googleSets(true);step();require(!spa.light&&!readyMode,"Service Mode refuses Google requests");require(!light.getOnOff()&&!wake.getOnOff(),"refused request snaps back");}
 else if(test=="fault-lockout"){raiseFault(FaultCode::OVERTEMP,true);light.googleSets(true);wake.googleSets(true);step();require(!spa.light&&!readyMode&&!light.getOnOff(),"active fault refuses Google starts");}
 else if(test=="light-from-dashboard"){run(3000);executeCommand("light on");step();require(light.getOnOff(),"local changes appear in Google Home");executeCommand("light off");step();require(!light.getOnOff(),"and off");}
 else if(test=="pairing"){step();step();int n=0;for(auto &e:liveEvents)if(e.id=="MATTER_PAIRING")n++;require(n==1,"pairing code recorded once in the event history");}
 else if(test=="colour-map"){for(uint8_t p=1;p<=LIGHT_PROGRAM_COUNT;++p){espHsvColor_t c=spaMatter::programHsv(p);require(lightProgramForColour((uint16_t)(c.h*360UL/254UL),c.s)==p,"each programme's wheel colour maps back to itself");}
  require(lightProgramForColour(200,30)==7,"pale colour -> White");require(lightProgramForColour(300,254)==5,"magenta -> Purple");require(lightProgramForColour(350,254)==1,"crimson -> Red");require(lightProgramForColour(165,254)==6,"teal -> Aqua");
  require(std::string(lightColourName(3))=="Blue"&&std::string(lightColourName(0))=="Unknown","names");}
 else if(test=="colour-on-from-off"){light.googleColour(spaMatter::programHsv(3).h,254);light.googleSets(true);step();require(spa.light&&lightProgram==3,"on + blue in one request starts on Blue");run(4000);require(pins[46],"lamp on after stepping");int n=0;for(auto &e:liveEvents)if(e.id=="MATTER_REQUEST")n++;require(n==1,"one command, not refused by the input lockout");}
 else if(test=="colour-while-on"){run(3000);executeCommand("light on");run(3000);light.googleColour(spaMatter::programHsv(2).h,254);step();require(lightProgram==2,"Green selected");run(3000);require(pins[46],"lamp back on after steps");espHsvColor_t c=light.getColorHSV();require(c.h==spaMatter::programHsv(2).h&&c.s==254,"wheel shows Green");}
 else if(test=="colour-in-lockout"){run(3000);executeCommand("light on");run(500);light.googleColour(spaMatter::programHsv(6).h,254);step();require(lightProgram==1,"waits out the 2 s lockout");run(2500);require(lightProgram==6,"then selects Aqua instead of refusing");}
 else if(test=="colour-snap"){run(3000);executeCommand("light on");run(3000);light.googleColour(10,254);step();require(lightProgram==1,"near-red stays Red");espHsvColor_t c=light.getColorHSV();require(c.h==spaMatter::programHsv(1).h,"wheel snaps to the real colour");}
 else if(test=="colour-from-dashboard"){run(3000);executeCommand("light on");run(3000);executeCommand("lightprog 5");step();espHsvColor_t c=light.getColorHSV();require(c.h==spaMatter::programHsv(5).h,"dashboard colour change shows in Google Home");run(5000);int n=0;for(auto &e:liveEvents)if(e.id=="MATTER_REQUEST")n++;require(n==0,"our own colour echo is not a request");}
 else if(test=="colour-while-off"){light.googleColour(spaMatter::programHsv(4).h,254);step();require(!spa.light&&lightWantedProgram==4,"colour while off is remembered, light stays off");executeCommand("light on");require(lightProgram==4,"next switch-on starts Yellow");}
 else if(test=="colour-service-lockout"){run(3000);executeCommand("light on");run(3000);setServiceMode(true);light.googleColour(spaMatter::programHsv(2).h,254);run(3000);require(lightProgram!=2,"Service Mode refuses colour");}
 else require(false,"unknown Matter scenario");
 for(int pin:{1,41})require(!pins[pin],"physical heater pins remain off");
 std::cout<<"PASS "<<test<<"\n";return 0;}
