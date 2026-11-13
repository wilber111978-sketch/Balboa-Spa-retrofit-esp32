#define main regression_main
#include "scenarios.cpp"
#undef main

void allOff(){for(int pin:{1,2,41,42,45,46})require(!pins[pin],"all six physical outputs OFF");}
int main(int argc,char**argv){
 std::string test=argc>1?argv[1]:"service";
 if(test=="settings-legacy-values"){Preferences::db["setpoint"]=39;Preferences::db["a_off"]=1;Preferences::db["start"]=22;Preferences::db["end"]=6;Preferences::db["heatmode"]=1;}
 if(test=="service-reboot"||test=="watchdog-reboot")importNVS(argv[2]);
 if(test=="watchdog-reboot")fakeResetReason=ESP_RST_TASK_WDT;
 if(test=="watchdog-panic")fakeResetReason=ESP_RST_PANIC;
 if(test=="brownout")fakeResetReason=ESP_RST_BROWNOUT;
 if(test=="watchdog-init-failure")fakeWdtInitResult=123;
 if(test=="watchdog-existing")fakeWdtInitResult=ESP_ERR_INVALID_STATE;
 if(test=="settings-open-failure")Preferences::failBegin=true;
 setup();
 if(test=="service"){
  executeCommand("low");executeCommand("light");run(2000);require(pins[2]&&pins[46],"loads active before service");
  require(executeCommand("service on").startsWith("OK"),"service acknowledged");allOff();
  executeCommand("heatmode solar");
#if SPA_BENCH_DEMO
  executeCommand("clock 11:00");
#else
  fakeClockValid=true;fakeMinute=660;
#endif
  afterUseRemainingMs=60000;run(10000);allOff();require(afterUseRemainingMs==60000,"service pauses after-use");
  for(const char*c:{"low","high","heat","light","ready","afteruse"})require(executeCommand(c).startsWith("REJECTED"),"service rejects start commands");
  require(spa.state==SpaState::SERVICE,"service state reported");require(executeCommand("service off").startsWith("ERROR"),"exit needs confirmation");
  require(executeCommand("service off confirm").startsWith("OK"),"explicit exit");run(2000);require(pins[2],"automatic LOW resumes after explicit exit");
 }
 else if(test=="alloff") {executeCommand("high");executeCommand("light");run(2000);executeCommand("alloff");allOff();run(5000);allOff();require(serviceMode,"alloff is persistent service");}
 else if(test=="off-semantics") {executeCommand("high");run(2000);require(executeCommand("off").s.find("automatic LOW")!=std::string::npos,"off response explains automatic LOW");run(2000);require(!pins[42]&&pins[2]&&afterUseRemainingMs>0,"off preserves after-use");}
 else if(test=="service-fault-exit") {raiseFault(FaultCode::OVERTEMP,true);executeCommand("service on");require(executeCommand("service off confirm").startsWith("REJECTED"),"fault prevents service exit");allOff();}
 else if(test=="service-save") {executeCommand("afteruse");executeCommand("alloff");exportNVS(argv[2]);require(serviceMode,"service saved for restart");}
 else if(test=="service-reboot") {require(serviceMode&&spa.fault==FaultCode::NONE,"service restored before operation");run(10000);allOff();require(afterUseRemainingMs==AFTER_USE_SEC*1000UL,"saved sanitation paused across restart");}
 else if(test=="watchdog-reboot"||test=="watchdog-panic") {require(serviceMode&&spa.fault==FaultCode::WATCHDOG_RESET&&spa.lockedOut,"watchdog/panic restart enters service and latches");run(1000);allOff();require(executeCommand("low").startsWith("REJECTED"),"recovered controller cannot auto restart loads");}
 else if(test=="brownout") {require(serviceMode&&spa.fault==FaultCode::POWER_FAULT,"brownout service lockout");allOff();}
 else if(test=="watchdog-feed") {int n=fakeWdtFeeds;run(3000);require(fakeWdtFeeds>n&&fakeWdtConfig.timeout_ms==5000&&fakeWdtConfig.trigger_panic,"watchdog registered and fed after loops");}
 else if(test=="watchdog-feed-failure") {executeCommand("low");executeCommand("light");run(2000);require(pins[46],"light active before watchdog failure");fakeWdtFeedResult=1;step();require(serviceMode&&spa.fault==FaultCode::WATCHDOG_RESET,"watchdog feed failure shuts down");allOff();}
 else if(test=="watchdog-init-failure") {require(serviceMode&&spa.fault==FaultCode::WATCHDOG_RESET,"watchdog startup failure fails closed");allOff();}
 else if(test=="watchdog-existing") {require(fakeWdtReconfigs==1&&controlWatchdog,"already initialized watchdog reconfigured");}
 else if(test=="settings-open-failure") {require(!storageHealthy&&serviceMode,"storage open failure fails closed");allOff();}
 else if(test=="settings-write-failure"||test=="settings-short-write"||test=="settings-readback") {
  executeCommand("low");run(2000);
  Preferences::failWrite=test=="settings-write-failure";Preferences::shortWrite=test=="settings-short-write";Preferences::badReadback=test=="settings-readback";
  require(executeCommand("setpoint 39").startsWith("ERROR"),"failed save never acknowledged OK");require(!storageHealthy&&serviceMode,"storage failure enters service");allOff();
 }
 else if(test=="settings-crc") {Preferences::blobs["config2"][8]^=1;loadConfiguration();require(!storageHealthy&&serviceMode,"checksum corruption detected");allOff();}
 else if(test=="settings-version") {ConfigRecord c{};readRecord("config2",1,c);c.version=99;c.crc=recordCRC(&c,offsetof(ConfigRecord,crc));auto &b=Preferences::blobs["config2"];memcpy(b.data(),&c,sizeof(c));loadConfiguration();require(!storageHealthy,"unsupported version rejected even with valid checksum");}
 else if(test=="settings-missing") {Preferences::blobs.erase("config2");loadConfiguration();require(!storageHealthy,"missing new-format record cannot fall back to legacy");}
 else if(test=="settings-truncated") {Preferences::blobs["config2"].resize(8);loadConfiguration();require(!storageHealthy,"truncated blob detected");}
 else if(test=="settings-semantic") {ConfigRecord record{};readRecord("config2",1,record);record.start=99;writeRecord("config2",1,record);loadConfiguration();require(!storageHealthy,"valid CRC does not bypass range validation");}
 else if(test=="settings-migration") {ConfigRecord c{};SafetyRecord safe{};RuntimeRecord r{};require(readRecord("config2",1,c)==RecordResult::VALID&&readRecord("safety2",2,safe)==RecordResult::VALID&&readRecord("runtime2",4,r)==RecordResult::VALID,"legacy/default migration creates checked records");}
 else if(test=="settings-roundtrip") {executeCommand("window 22-6");executeCommand("setpoint 39");executeCommand("heatmode solar");config.setpointC=20;solarStart=1;solarEnd=2;heatMode=0;loadConfiguration();require(config.setpointC==39&&solarStart==22&&solarEnd==6&&heatMode==1,"atomic settings round trip");}
 else if(test=="safety-crc") {Preferences::blobs["safety2"][4]^=1;loadSafety();require(serviceMode&&!storageHealthy,"corrupt service/fault record fails closed");allOff();}
 else if(test=="runtime-crc") {Preferences::blobs["runtime2"][8]^=1;loadRuntime();require(serviceMode&&!storageHealthy,"corrupt runtime detected");allOff();}
 else if(test=="fault-reset-save-failure") {raiseFault(FaultCode::TEMP_A_INVALID,false);Preferences::failWrite=true;require(executeCommand("resetfault").startsWith("ERROR"),"failed fault clear not acknowledged");require(spa.fault!=FaultCode::NONE&&serviceMode,"failed fault clear remains stopped");allOff();}
 else if(test=="service-exit-save-failure") {executeCommand("alloff");Preferences::failWrite=true;require(executeCommand("service off confirm").startsWith("REJECTED"),"failed service exit not acknowledged");require(serviceMode,"failed service exit stays stopped");allOff();}
 else if(test=="stale-before-refresh") {executeCommand("low");run(2000);fakeMs+=2001;loop();require(spa.fault==FaultCode::SENSOR_STALE&&!pins[2]&&!pins[42],"late loop faults before sensor refresh");require(executeCommand("resetfault").startsWith("OK"),"fresh safe sensors can acknowledge stale fault");}
 else if(test=="stale-b") {healthy();tempB.lastUpdateMs=millis()-2001;checkSensorFreshness();require(spa.fault==FaultCode::SENSOR_STALE,"B-only stale channel detected");}
 else if(test=="stale-wrap") {healthy();tempA.lastUpdateMs=tempB.lastUpdateMs=UINT32_MAX-500;fakeMs=1600;checkSensorFreshness();require(spa.fault==FaultCode::SENSOR_STALE,"stale timer works across millis wrap");}
 else if(test=="network-slow-header") {executeCommand("low");run(2000);int id=MockSockets::connect("POST /api/cmd HTTP/1.1\r\n");run(2000);require(!MockSockets::sockets[id]->active&&web.expired>0,"slow header expires");require(pins[2]&&spa.fault==FaultCode::NONE,"network stall does not block control");}
 else if(test=="network-slow-body") {executeCommand("primeend");int id=MockSockets::connect("POST /api/cmd HTTP/1.1\r\nContent-Length: 8\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\ncmd=");run(2000);require(!MockSockets::sockets[id]->active&&spa.requestedPump==PumpMode::OFF,"partial body expires without command");}
 else if(test=="network-budget") {int id=MockSockets::connect(std::string(1400,'x'));processWeb();require(web.lastRxBytes<=128&&MockSockets::sockets[id]->input.size()==1272,"RX byte budget enforced");}
 else if(test=="network-large-header") {int id=MockSockets::connect(std::string(1200,'x'));for(int i=0;i<30&&MockSockets::sockets[id]->active;i++){fakeMs++;processWeb();}require(web.response==431,"oversized header rejected");}
 else if(test=="network-stalled-response") {int id=MockSockets::connect("GET / HTTP/1.1\r\n\r\n");MockSockets::sockets[id]->blockWrite=true;run(2000);require(!MockSockets::sockets[id]->active&&web.expired>0,"stalled response expires");require(spa.fault==FaultCode::NONE,"TX stall does not block sensors");}
 else if(test=="network-auth-status") {std::string out=httpRequest("",false,true);require(out.find("401")!=std::string::npos,"status requires authentication");out=httpRequest("",true,true);require(out.find("heaterPinsInhibited")!=std::string::npos&&out.find("settingsHealthy")!=std::string::npos,"authenticated JSON status");}
 else if(test=="network-duplicate-length") {int id=MockSockets::connect("POST /api/cmd HTTP/1.1\r\nContent-Length: 8\r\nContent-Length: 8\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\ncmd=high");for(int i=0;i<20&&MockSockets::sockets[id]->active;i++){fakeMs++;processWeb();}require(web.response==400&&spa.requestedPump==PumpMode::OFF,"ambiguous request cannot execute");}
 else if(test=="network-service") {httpRequest("cmd=alloff");require(serviceMode&&web.response==200,"remote alloff enters service");httpRequest("cmd=high");require(web.response==400,"remote start blocked in service");allOff();}
 else if(test=="network-tx-budget") {int id=MockSockets::connect("GET / HTTP/1.1\r\n\r\n");processWeb();require(web.lastTxBytes<=128&&MockSockets::sockets[id]->output.size()<=128,"TX byte budget enforced");}
 else if(test=="crc-known-vector") {require(recordCRC("123456789",9)==0xCBF43926UL,"CRC32 matches standard known vector");}
 else if(test=="settings-legacy-values") {require(config.setpointC==39&&config.sensorAOffset==1&&solarStart==22&&solarEnd==6&&heatMode==1,"valid existing settings preserved during migration");}
 else {require(false,"unknown extended scenario");}
 std::cout<<"PASS "<<SPA_BENCH_DEMO<<" "<<test<<"\n";
}
