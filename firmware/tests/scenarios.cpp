#ifndef SPA_BENCH_DEMO
#define SPA_BENCH_DEMO 1
#endif
#include "../libraries/SpaController/src/SpaController.h"
#include <iostream>
#include <cassert>
#include <fstream>
void require(bool ok,const char*msg){if(!ok){std::cerr<<"FAIL: "<<msg<<"\n";std::exit(1);}}
void step(uint32_t n=250){fakeMs+=n;loop();require(!(pins[2]&&pins[42]),"pump pins mutually exclusive");require(!pins[1]&&!pins[41],"physical heater pins OFF");}
void run(uint32_t n){for(uint32_t x=0;x<n;x+=250)step();}
void healthy(){tempA.status=tempB.status=SensorStatus::OK;tempA.rawC=tempB.rawC=tempA.filteredC=tempB.filteredC=36;tempA.lastUpdateMs=tempB.lastUpdateMs=millis();spa.waterTemperature=36;spa.tempDifference=0;}
std::string httpRequest(std::string body,bool authorised=true,bool get=false) {
 std::string request=(get?"GET /api/status HTTP/1.1\r\n":"POST /api/cmd HTTP/1.1\r\n");
 if(authorised)request+="Authorization: Basic c3BhOnRlc3QtcGFzc3dvcmQtb25seQ==\r\n";
 if(!get)request+="Content-Type: application/x-www-form-urlencoded\r\nContent-Length: "+std::to_string(body.size())+"\r\n";
 int id=MockSockets::connect(request+"\r\n"+body);
 for(int i=0;i<100&&MockSockets::sockets[id]->active;i++){fakeMs+=1;processWeb();}
 return MockSockets::sockets[id]->output;
}
void exportNVS(const char*path){std::ofstream f(path);for(auto &kv:Preferences::db)f<<kv.first<<" "<<kv.second<<"\n";for(auto &kv:Preferences::blobs){f<<"@"<<kv.first<<" ";for(uint8_t b:kv.second){static const char h[]="0123456789abcdef";f<<h[b>>4]<<h[b&15];}f<<"\n";}}
void importNVS(const char*path){std::ifstream f(path);std::string k,v;while(f>>k>>v){if(k[0]=='@'){std::vector<uint8_t> bytes;for(size_t i=0;i<v.size();i+=2)bytes.push_back((uint8_t)strtoul(v.substr(i,2).c_str(),nullptr,16));Preferences::blobs[k.substr(1)]=bytes;}else Preferences::db[k]=std::stod(v);}}
int main(int argc,char**argv){
 std::string test=argc>1?argv[1]:"startup";
 if(test=="corrupt-config")Preferences::db["a_beta"]=NAN;
 if(test=="restart-fault")Preferences::db["fault"]=(int)FaultCode::OVERTEMP;
 if(test=="restart-manual")Preferences::db["heatmode"]=2;
 if(test=="restart-afteruse")Preferences::db["afteruse"]=120000;
 if(test=="adc-startup")ads.connected=false;
 if(test=="power-recover"&&argc>2)importNVS(argv[2]);
 if(test=="rollover"||test=="runon-wrap")fakeMs=UINT32_MAX-4000;
 setup();
#if !SPA_BENCH_DEMO
 if(test=="main-injections"){require(executeCommand("overtemp").startsWith("ERROR"),"Main rejects injection");require(executeCommand("clock 11:00").startsWith("ERROR"),"Main rejects demo clock");}
 else if(test=="main-lock"){executeCommand("heatmode manual");executeCommand("low");run(150000);require(!spa.heaterSSR&&!spa.heaterSafetyContactor,"Main lockout");}
 else if(test=="adc-startup"){require(spa.fault==FaultCode::ADC_FAILURE&&spa.lockedOut,"ADC startup lockout");}
 else if(test=="adc-disconnect"){Wire.connected=false;run(1000);require(spa.fault==FaultCode::ADC_FAILURE,"ADC disconnect fault");}
 else if(test=="adc-timeout"){ads.stuck=true;run(1000);require(spa.fault==FaultCode::ADC_FAILURE,"bounded conversion timeout");}
 else if(test=="ntc"){require(fabs(resistanceToTemperatureC(30000,30000,3860)-25)<0.01,"NTC at 25C");require(fabs(voltageToResistance(1.65)-30000)<1,"divider");updateTemperatureChannel(tempA,0,30000,3860,0);require(tempA.status==SensorStatus::SHORT,"short");updateTemperatureChannel(tempB,26000,30000,3860,0);require(tempB.status==SensorStatus::OPEN,"open");}
 else if(test=="main-ema"){updateTemperatureChannel(tempA,13200,30000,3860,0);fakeMs+=500;updateTemperatureChannel(tempA,10000,30000,3860,0);require(tempA.rawC>tempA.filteredC&&tempA.rateCPerSec>0,"real EMA and slope");}
 else if(test=="raw-overtemp"){healthy();tempA.rawC=43;temperatureSystemHealthy();require(spa.fault==FaultCode::OVERTEMP&&spa.lockedOut,"raw overtemp bypasses EMA");}
#else
 if(test=="startup"){require(spa.actualPump==PumpMode::OFF&&spa.fault==FaultCode::NONE,"safe startup");require(!pins[1]&&!pins[41],"safe heater startup");}
 else if(test=="heat"){executeCommand("heatmode manual");executeCommand("ready");run(12000);require(spa.heaterSSR&&spa.heaterSafetyContactor,"logical heat qualified");require(!pins[1]&&!pins[41],"demo never drives heater");demoTempA=demoTempB=38;run(1000);require(!spa.heaterSSR,"setpoint stops heating");}
 else if(test=="hysteresis"){executeCommand("heatmode manual");demoTempA=demoTempB=36;run(12000);require(!spa.heaterSSR,"standby 3C band");executeCommand("ready");run(10000);require(spa.heaterSSR,"ready 1C band");}
 else if(test=="changeover"){executeCommand("low");run(2000);require(pins[2],"LOW on");executeCommand("high");step();require(!pins[2]&&!pins[42],"both off deadtime");step(500);require(!pins[42],"deadtime at 500ms");step(250);require(pins[42],"HIGH at 750ms");}
 else if(test=="fault-high"){executeCommand("high");run(2000);executeCommand("sensorafail");run(1000);require(!pins[2]&&!pins[42]&&!pins[45],"sensor fault stops pump UV");require(executeCommand("high").startsWith("REJECTED"),"fault rejects HIGH");}
 else if(test=="sensor-b"){executeCommand("sensorbfail");run(1000);require(spa.fault==FaultCode::TEMP_B_INVALID,"B invalid");}
 else if(test=="overtemp"){executeCommand("heatmode manual");executeCommand("ready");run(12000);executeCommand("overtemp");run(1000);require(spa.lockedOut&&!spa.heaterSSR,"overtemp wins heat");require(executeCommand("resetfault").startsWith("REJECTED"),"unsafe reset refused");executeCommand("normal");require(executeCommand("resetfault").startsWith("OK"),"safe reset accepted");SafetyRecord saved{};require(readRecord("safety2",2,saved)==RecordResult::VALID&&!saved.fault,"persistent latch cleared");}
 else if(test=="disagreement"){executeCommand("diff");run(11000);require(spa.fault==FaultCode::TEMP_DISAGREE,"persistent disagreement fault");}
 else if(test=="diff-inhibit"){executeCommand("heatmode manual");executeCommand("ready");run(12000);demoTempB=37.6;run(1000);require(!spa.heaterSSR,"1.5C difference inhibits heat");}
 else if(test=="highlimit"){executeCommand("highlimit");run(1000);require(spa.fault==FaultCode::HIGH_LIMIT_OPEN&&spa.lockedOut,"limit trips even idle");}
 else if(test=="invalid-range"){executeCommand("tempa -1");run(1000);require(spa.fault==FaultCode::TEMP_A_INVALID,"demo enforces sensor range");}
 else if(test=="filtration"){executeCommand("clock 10:59");run(1000);require(spa.actualPump==PumpMode::OFF,"before solar window");executeCommand("clock 11:00");run(2000);require(spa.actualPump==PumpMode::LOW_SPEED&&pins[45],"11AM filtration UV");executeCommand("clock 16:00");run(2000);require(spa.actualPump==PumpMode::OFF,"16:00 exclusive");}
 else if(test=="schedule-heat"){dailyLowSec=DAILY_FILTER_TARGET_SEC;executeCommand("heatmode solar");executeCommand("clock 10:59");require(!schedulePermitsHeating(),"10:59 no heat");executeCommand("clock 11:00");require(schedulePermitsHeating(),"demo clock used by heating");executeCommand("clock 16:00");require(!schedulePermitsHeating(),"16:00 no heat");}
 else if(test=="overnight"){executeCommand("window 22-6");executeCommand("heatmode solar");executeCommand("clock 23:00");executeCommand("ready");require(schedulePermitsHeating(),"overnight before midnight");run(2000);require(spa.requestedPump==PumpMode::LOW_SPEED,"overnight circulation");executeCommand("clock 03:00");require(schedulePermitsHeating(),"overnight after midnight");executeCommand("clock 06:00");require(!schedulePermitsHeating(),"overnight end");}
 else if(test=="equal-window"){executeCommand("window 11-11");executeCommand("clock 11:00");executeCommand("heatmode solar");run(2000);require(!schedulePermitsHeating()&&spa.actualPump==PumpMode::OFF,"equal window closed");}
 else if(test=="catchup"){executeCommand("clock 13:00");run(2000);require(spa.actualPump==PumpMode::LOW_SPEED,"catchup quota");dailyLowSec=DAILY_FILTER_TARGET_SEC;run(2000);require(spa.actualPump==PumpMode::OFF,"quota stops filtration");}
 else if(test=="afteruse"){executeCommand("high");run(2000);executeCommand("off");run(2000);require(afterUseRemainingMs>0&&spa.actualPump==PumpMode::LOW_SPEED&&pins[45],"jets trigger LOW UV");uint32_t before=afterUseRemainingMs;run(10000);require(before-afterUseRemainingMs>=9500,"counts actual LOW time");run(AFTER_USE_SEC*1000UL);require(!afterUseRemainingMs&&spa.actualPump==PumpMode::OFF,"after-use finishes");}
 else if(test=="afteruse-pause"){executeCommand("afteruse");run(2000);executeCommand("sensorafail");run(1000);uint32_t n=afterUseRemainingMs;run(10000);require(afterUseRemainingMs==n,"fault pauses after-use");executeCommand("normal");executeCommand("resetfault");run(2000);require(spa.actualPump==PumpMode::LOW_SPEED,"after-use resumes after reset");}
 else if(test=="jets-timeout"){executeCommand("high");run(JETS_TIMEOUT_MS+5000);require(spa.actualPump==PumpMode::LOW_SPEED&&afterUseRemainingMs>0,"jets 20 minute timeout");}
 else if(test=="ready-timeout"){executeCommand("ready");run(READY_TIMEOUT_MS+1000);require(!readyMode,"ready expires");}
 else if(test=="runtime"){executeCommand("low");run(62000);require(dailyLowSec>=60&&dailyLowSec<=62&&sinceCleanLowSec==dailyLowSec,"runtime seconds accounting");saveRuntime();RuntimeRecord saved{};require(readRecord("runtime2",4,saved)==RecordResult::VALID&&saved.seconds==dailyLowSec,"runtime persisted");}
 else if(test=="day-rollover"){executeCommand("clock 23:59");dailyLowSec=100;run(2000);dailyLowSec=100;run(61000);require(dailyLowSec==0,"midnight reset");}
 else if(test=="nonfinite"){healthy();tempA.filteredC=NAN;require(!temperatureSystemHealthy(),"nonfinite filtered sensor fails closed");}
 else if(test=="filter-threshold"){sinceCleanLowSec=FILTER_CLEAN_SEC;require(statusJSON().s.find("\"filterCleanDue\":true")!=std::string::npos,"56 hour reminder");sinceCleanLowSec=0;cleanEpoch=uint32_t(time(nullptr))-15*86400;require(cleanAgeDays()>=14,"calendar clean age");require(statusJSON().s.find("\"filterCleanDue\":true")!=std::string::npos,"14 day reminder");}
 else if(test=="clock-backwards"){executeCommand("clock 11:00");run(2000);dailyLowSec=1234;demoDay=0;executeCommand("clock 10:00");run(1000);require(dailyLowSec>=1234,"backward day correction does not reset quota");}
 else if(test=="stale"){healthy();fakeMs+=3000;require(!temperatureSystemHealthy(),"stale measurements refused");}
 else if(test=="json"){executeCommand("sensorafail");run(1000);std::cout<<statusJSON().c_str()<<"\n";require(statusJSON().s.find("nan")==std::string::npos,"JSON null for invalid sensors");}
 else if(test=="rapid-commands"){uint32_t seed=1234;const char*cmds[]={"low","high","off","heatmode manual","heatmode off","afteruse","ready","standby"};for(int i=0;i<10000;i++){seed=seed*1664525+1013904223;executeCommand(cmds[seed%8]);step(25);}require(!spa.lockedOut,"no false fault in command stress");}
 else if(test=="rollover"){fakeMs=UINT32_MAX-2000;executeCommand("low");run(5000);require(spa.actualPump==PumpMode::LOW_SPEED,"pump timer across millis wrap");}
 else if(test=="wifi-loss"){executeCommand("heatmode manual");executeCommand("ready");run(12000);WiFi.state=0;run(35000);require(spa.heaterSSR,"control independent of WiFi status");require(WiFi.reconnects>0,"WiFi reconnect attempted");executeCommand("sensorafail");run(1000);require(!spa.heaterSSR,"fault wins during WiFi loss");}
 else if(test=="malformed"){require(executeCommand("setpoint 39garbage").startsWith("ERROR"),"strict setpoint");require(executeCommand("window abc-def").startsWith("ERROR"),"strict window");require(executeCommand("tempa garbage").startsWith("ERROR"),"strict demo temp");require(executeCommand("clock aa:bb").startsWith("ERROR"),"strict clock");}
 else if(test=="power-save"){executeCommand("heatmode manual");executeCommand("high");run(5000);executeCommand("sensorafail");run(1000);exportNVS(argv[2]);require(spa.fault!=FaultCode::NONE,"fault saved before simulated power cut");}
 else if(test=="power-recover"){require(spa.lockedOut&&spa.fault!=FaultCode::NONE,"fault restored in fresh process");require(heatMode==0&&afterUseRemainingMs>0,"manual heat off, sanitation restored after power cut");run(5000);require(!pins[1]&&!pins[41]&&!pins[2]&&!pins[42],"fault blocks recovered demands");}
 else if(test=="web-auth"){httpRequest("cmd=high",false);require(web.response==401&&spa.requestedPump==PumpMode::OFF,"unauthorised request blocked");httpRequest("cmd=high");run(2000);require(pins[42],"authenticated HIGH");executeCommand("sensorafail");run(1000);httpRequest("cmd=high");require(web.response==400&&!pins[42],"fault rejects authenticated command");}
 else if(test=="web-input"){httpRequest("");require(web.response==400,"missing command");httpRequest("cmd="+std::string(97,'x'));require(web.response==400,"oversized command");}
 else if(test=="heat-runon"){executeCommand("heatmode manual");executeCommand("ready");run(12000);require(spa.heaterSSR,"heat initially on");executeCommand("heatmode off");run(1000);require(!spa.heaterSSR&&spa.actualPump==PumpMode::LOW_SPEED,"heat off keeps runon");run(32000);require(spa.actualPump==PumpMode::OFF,"runon ends");}
 else if(test=="fault-v-schedule"){executeCommand("heatmode solar");executeCommand("clock 11:00");executeCommand("ready");run(12000);require(spa.heaterSSR,"schedule initially heating");executeCommand("sensorafail");run(1000);executeCommand("heatmode manual");run(2000);require(!spa.heaterSSR&&!pins[2]&&!pins[42],"fault wins manual and schedule");}
 else if(test=="rapid-rise"){executeCommand("heatmode manual");executeCommand("ready");run(15000);require(spa.heaterSSR,"heat on before rate test");demoTempA=demoTempB=37;run(500);require(spa.fault==FaultCode::RAPID_TEMP_RISE&&spa.lockedOut,"rapid rise lockout");}
 else if(test=="runon-wrap"){fakeMs=UINT32_MAX-1000;executeCommand("heatmode off");run(5000);require(spa.actualPump==PumpMode::OFF,"no phantom runon near wrap");}
 else if(test=="clock-loss"){heatMode=1;fakeClockValid=false;demoMinuteOfDay=-1;run(2000);require(!schedulePermitsHeating()&&spa.actualPump==PumpMode::OFF,"unknown clock fails closed");}
 else if(test=="clean-confirm"){sinceCleanLowSec=FILTER_CLEAN_SEC;require(executeCommand("filtercleaned").startsWith("ERROR"),"clean confirmation required");executeCommand("filtercleaned confirm");require(!sinceCleanLowSec,"clean counter reset");}
#endif
 if(test=="corrupt-config")require(spa.fault==FaultCode::CONFIG_CORRUPT&&spa.lockedOut,"corrupt calibration lockout");
 if(test=="restart-fault")require(spa.fault==FaultCode::OVERTEMP&&spa.lockedOut&&!pins[1],"fault survives restart");
 if(test=="restart-manual")require(heatMode==0&&spa.actualPump==PumpMode::OFF,"manual demand not resumed");
 if(test=="restart-afteruse")require(afterUseRemainingMs==120000,"after-use restored");
 std::cout<<"PASS "<<SPA_BENCH_DEMO<<" "<<test<<"\n";
 return 0;
}
