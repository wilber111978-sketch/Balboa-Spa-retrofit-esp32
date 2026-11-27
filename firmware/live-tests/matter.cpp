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
 else require(false,"unknown Matter scenario");
 for(int pin:{1,41})require(!pins[pin],"physical heater pins remain off");
 std::cout<<"PASS "<<test<<"\n";return 0;}
