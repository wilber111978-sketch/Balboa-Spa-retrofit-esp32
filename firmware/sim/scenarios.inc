// Scenario definitions for fw_sim.cpp. Times in seconds of simulated time.
#undef AT
#define AT(t, ...) Act{(double)(t), []{ __VA_ARGS__; }}

static bool cleanInv(){ return !M.lowHigh && !M.heatNoLow && !M.heatHigh && !M.chatter && M.uvNoPump < 0.5 && M.heatPriming == 0; }
static std::string inv(){ return F("inv lowHigh=%ld heatNoLow=%ld heatHigh=%ld chatter=%ld uvNoPump=%.1fs heatPriming=%.1fs", M.lowHigh, M.heatNoLow, M.heatHigh, M.chatter, M.uvNoPump, M.heatPriming); }
static void heatManual(double sp){ cmd("heatmode manual"); cmd(F("setpoint %.1f", sp).c_str()); cmd("ready"); }
static void heatSolar(double sp){ cmd("heatmode solar"); cmd("window 11-16"); cmd(F("setpoint %.1f", sp).c_str()); }
static void endPriming(){ cmd("primeend"); }

void defineScenarios(){
 // ============================ normal operation
 S.push_back({"N01-heatup-manual-30to38", 12*H_, 0.1, []{P.Ttub=30;},
  {AT(1, heatManual(38))},
  []{bool ok=cleanInv()&&M.maxTub>=37.85&&M.maxTub<=38.6&&!M.firstFaultCode&&M.dry==0;
     return expect(ok, F("final %.2f max %.2f heater %.2fh; %s", P.Ttub, M.maxTub, M.heaterOn/3600, inv().c_str()));}});
 S.push_back({"N02-solar-week-mild", 7*24*H_, 0.2, []{P.Ttub=36;P.profile=true;P.dayAmb=24;P.nightAmb=14;P.sunPeak=300;P.startMin=0;},
  {AT(1, heatSolar(38))},
  []{bool ok=cleanInv()&&!M.firstFaultCode&&M.minTub>=33.5&&M.maxTub<=39.5&&M.lowOn/3600>=7*3.5;
     return expect(ok, F("tub min %.2f max %.2f; LOW %.1fh/wk (%.1fh/day); heater %.1fh; pump starts %ld; safety ops %ld; %s", M.minTub, M.maxTub, M.lowOn/3600, M.lowOn/3600/7, M.heaterOn/3600, M.pumpStarts, M.safetyOps, inv().c_str()));}});
 S.push_back({"N03-solar-week-winter", 7*24*H_, 0.2, []{P.Ttub=36;P.profile=true;P.dayAmb=16;P.nightAmb=4;P.sunPeak=150;P.startMin=0;},
  {AT(1, heatSolar(38))},
  []{bool ok=cleanInv()&&!M.firstFaultCode&&M.minTub>=33.5;
     return expect(ok, F("tub min %.2f max %.2f final %.2f; floor used=%d; heater %.1fh; %s", M.minTub, M.maxTub, P.Ttub, M.floorSeen, M.heaterOn/3600, inv().c_str()));}});
 S.push_back({"N04-hot-3-days", 3*24*H_, 0.2, []{P.Ttub=38;P.profile=true;P.dayAmb=38;P.nightAmb=26;P.sunPeak=600;P.startMin=0;},
  {AT(1, heatSolar(38))},
  []{bool ok=cleanInv()&&!seen(FaultCode::OVERTEMP)&&M.waterHotSeen&&M.heaterOn==0;
     return expect(ok, F("no firmware over-temp latch; waterHot warning=%d; tub max %.2f; hardware high-limit tripped by hot water=%d (fault %d at %.1fh); heater %.2fh", M.waterHotSeen, M.maxTub, P.hlTripped, M.firstFaultCode, M.firstFaultT/3600, M.heaterOn/3600));}});
 S.push_back({"N05-jets-during-heating", 4*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(40*MIN_, cmd("high"))},
  []{bool ok=cleanInv()&&M.highOn>=19*60&&M.highOn<=21*60&&!M.firstFaultCode&&P.Ttub>=37.5;
     return expect(ok, F("HIGH %.1f min (20 min timeout); final %.2f; %s", M.highOn/60, P.Ttub, inv().c_str()));}});
 S.push_back({"N06-button-mash", 2*H_, 0.1, []{P.Ttub=37;},
  {AT(1, heatManual(38)), AT(30*MIN_, for(int i=0;i<20;i++){fakeMs+=500;cmd(i%2?"high":"low");})},
  []{return expect(cleanInv()&&!M.firstFaultCode, F("no faults; pump starts %ld; %s", M.pumpStarts, inv().c_str()));}});
 S.push_back({"N07-manual-low-at-night", 12*H_, 0.2, []{P.Ttub=37;P.startMin=21*60+30;},
  {AT(5*MIN_, cmd("low"))},
  []{return expect(cleanInv()&&M.lowOn/3600<2.0, F("LOW total %.2fh incl. priming+polls (manual stops at 20 min); %s", M.lowOn/3600, inv().c_str()));}});
 S.push_back({"N08-light-colour-cycle", 2*H_, 0.1, []{P.Ttub=37;},
  {AT(10, endPriming()), AT(20, cmd("light on")), AT(30, cmd("lightprog 3")), AT(60, cmd("light off")), AT(63, cmd("light on")), AT(90, cmd("lightprog 5"))},
  []{bool ok=M.lightDesync==0&&!P.lightPowered&&P.lightProg==5;
     return expect(ok, F("desync %.1fs; light program %d (want 5, off after 40 min)", M.lightDesync, P.lightProg));}});
 S.push_back({"N09-light-reset-longer-than-assumed", 30*MIN_, 0.1, []{P.Ttub=37;P.lightResetOff=12;},
  {AT(10, endPriming()), AT(20, cmd("light on")), AT(30, cmd("lightprog 3")), AT(60, cmd("light off")), AT(71, cmd("light on"))},
  []{return expect(true, F("INFO real reset 12 s vs firmware 10 s: desync %.1fs (set LIGHT_RESET_OFF_MS from bench measurement)", M.lightDesync));}});
 S.push_back({"N10-light-with-colour-memory", 30*MIN_, 0.1, []{P.Ttub=37;P.lightMemory=true;},
  {AT(10, endPriming()), AT(20, cmd("light on")), AT(30, cmd("lightprog 3")), AT(60, cmd("light off")), AT(200, cmd("light on"))},
  []{return expect(true, F("INFO light that remembers colour: desync %.1fs, light on %d vs firmware %d", M.lightDesync, P.lightProg, lightProgram));}});

 // ============================ power events
 S.push_back({"P01-3s-power-cut-heating", 5*H_, 0.1, []{P.Ttub=34;P.startMin=10*60+50;},
  {AT(1, heatSolar(38)), AT(1, cmd("ready")), AT(60*MIN_, P.mains=false; outageEnd=simT+3), AT(62*MIN_, cmd("ready"))},
  []{return expect(cleanInv()&&M.primingSeen&&!M.firstFaultCode&&M.maxTub>=37.85&&heatMode==1, F("solar mode survived reboot=%d, primed, max %.2f final %.2f; %s", heatMode==1, M.maxTub, P.Ttub, inv().c_str()));}});
 S.push_back({"P02-0.4s-blip-light-on", 30*MIN_, 0.1, []{P.Ttub=37;},
  {AT(10, endPriming()), AT(20, cmd("light on")), AT(30, cmd("lightprog 4")), AT(10*MIN_, P.mains=false; outageEnd=simT+0.4), AT(10*MIN_+8, cmd("light on"))},
  []{return expect(cleanInv()&&M.lightDesync==0, F("after reboot light desync %.1fs (program %d vs fw %d)", M.lightDesync, P.lightProg, lightProgram));}});
 S.push_back({"P03-24V-sag-while-heating", 2*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(60*MIN_, P.v24=14), AT(60*MIN_+5, P.v24=24)},
  []{return expect(cleanInv()&&!M.lockoutSeen, F("UV on during sag %.1fs (hardware limit, no 24 V monitor in onboard build); contactors dropped and recovered; uncontrolled load breaks %ld; faults %d; final %.2f; %s", M.uvNoPumpSag, M.loadBreak, M.firstFaultCode, P.Ttub, inv().c_str()));}});
 S.push_back({"P04-power-cut-keeps-latched-lockout", 2*H_, 0.1, []{P.Ttub=36;},
  {AT(1, heatManual(38)), AT(30*MIN_, P.sB={4,7.0}), AT(45*MIN_, P.mains=false; outageEnd=simT+5)},
  []{return expect(spa.lockedOut&&seen(FaultCode::OVERTEMP), F("lockout after reboot=%d fault=%d", spa.lockedOut, (int)spa.fault));}});

 // ============================ sensors
 S.push_back({"S01-sensorA-open-heating", 1*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(20*MIN_, P.sA={1,0})},
  []{return expect(cleanInv()&&seen(FaultCode::TEMP_A_OPEN)&&!P.heater&&M.dry==0, F("fault at %.0fs; %s", M.faultFirstT[(int)FaultCode::TEMP_A_OPEN], inv().c_str()));}});
 S.push_back({"S02-sensorB-short-heating", 1*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(20*MIN_, P.sB={2,0})},
  []{return expect(cleanInv()&&seen(FaultCode::TEMP_B_SHORT)&&!P.heater, F("fault at %.0fs; %s", M.faultFirstT[(int)FaultCode::TEMP_B_SHORT], inv().c_str()));}});
 S.push_back({"S03-both-sensors-frozen-30C", 24*H_, 0.2, []{P.Ttub=30;},
  {AT(1, heatManual(38)), AT(10*MIN_, P.sA={3,30}; P.sB={3,30})},
  []{bool ok=cleanInv()&&M.maxTub<=39.0&&seen(FaultCode::HEATER_CONTACTOR_FAULT);
     return expect(ok, F("readings frozen at 30 C while heating (no-response check): max tub %.2f, heater body %.1f, high-limit %d, faults %d; %s", M.maxTub, M.maxHeater, P.hlTripped, M.firstFaultCode, inv().c_str()));}});
 S.push_back({"S04-sensorA-reads-0.9C-low", 24*H_, 0.2, []{P.Ttub=34;P.sA={4,-0.9};},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.maxTub<=38.6&&M.mismatchSeen, F("mismatch inhibit=%d max tub %.2f; %s", M.mismatchSeen, M.maxTub, inv().c_str()));}});
 S.push_back({"S05-sensorA-reads-0.4C-low", 24*H_, 0.2, []{P.Ttub=34;P.sA={4,-0.4};},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.maxTub<=38.8, F("below 0.5 C threshold: max tub %.2f; %s", M.maxTub, inv().c_str()));}});
 S.push_back({"S06-sensorB-reads-2C-high", 6*H_, 0.2, []{P.Ttub=34;P.sB={4,2.0};},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.maxTub<=38.8, F("heat inhibited=%d; faults %d; max tub %.2f; heater %.2fh", M.mismatchSeen, M.firstFaultCode, M.maxTub, M.heaterOn/3600));}});
 S.push_back({"S07-sensors-wired-swapped", 8*H_, 0.1, []{P.Ttub=34;P.swapAB=true;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&!M.firstFaultCode&&M.maxTub>=37.85&&M.maxTub<=38.6, F("max %.2f final %.2f; %s", M.maxTub, P.Ttub, inv().c_str()));}});
 S.push_back({"S08-onboard-adc-gain-error-2pct", 8*H_, 0.1, []{P.Ttub=34;P.adcGain=1.02;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&!M.firstFaultCode, F("INFO uncalibrated: water ends %.2f for setpoint 38 (display error %.2f) -> one-point calibration needed", P.Ttub, M.maxDispErr));}});
 S.push_back({"S09-onboard-adc-noise-3mV", 8*H_, 0.1, []{P.Ttub=34;P.adcNoiseMv=3;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&!M.firstFaultCode&&M.maxTub>=37.6, F("noisy ADC: final %.2f faults %d; %s", P.Ttub, M.firstFaultCode, inv().c_str()));}});
 S.push_back({"S12-onboard-adc-noise-6mV-stress", 8*H_, 0.1, []{P.Ttub=34;P.adcNoiseMv=6;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv(), F("INFO stress: final %.2f, faults %s first %d at %.0fs, lockout %d", P.Ttub, "", M.firstFaultCode, M.firstFaultT, M.lockoutSeen));}});
 S.push_back({"S10-sensorA-intermittent-open", 6*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(30*MIN_, P.sA={5,0})},
  []{return expect(cleanInv()&&spa.lockedOut, F("repeating dropout latches after 3 in 24 h: lockout=%d; first fault %.0fs", spa.lockedOut, M.firstFaultT));}});
 S.push_back({"S11-sensorA-single-glitch-recovers", 6*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(30*MIN_, P.sA={1,0}), AT(30*MIN_+1, P.sA={0,0})},
  []{return expect(cleanInv()&&spa.fault==FaultCode::NONE&&!spa.lockedOut&&M.maxTub>=37.85, F("single glitch auto-cleared; heating resumed, max %.2f", M.maxTub));}});

 // ============================ flow
 S.push_back({"F01-filter-85pct-blocked-heating", 3*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(30*MIN_, P.flowFactor=0.15)},
  []{return expect(cleanInv()&&M.maxHeater<44, F("retry=%d lockout=%d max heater body %.1f dry %.1fs faults %d; %s", M.flowRetrySeen, M.lockoutSeen, M.maxHeater, M.dry, M.firstFaultCode, inv().c_str()));}});
 S.push_back({"F02-flow-lost-heating", 3*H_, 0.1, []{P.Ttub=34;},
  {AT(1, heatManual(38)), AT(30*MIN_, P.flowFactor=0.0)},
  []{return expect(cleanInv()&&M.lockoutSeen&&M.maxHeater<44, F("retries then lockout=%d; max heater body %.1f; dry %.1fs; %s", M.lockoutSeen, M.maxHeater, M.dry, inv().c_str()));}});
 S.push_back({"F03-airlock-after-refill", 3*H_, 0.1, []{P.Ttub=25;P.airlockS=600;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.maxHeater<44, F("air cleared=%d; retry=%d lockout=%d; max heater %.1f dry %.1fs; final %.2f", P.airlockS<=0, M.flowRetrySeen, M.lockoutSeen, M.maxHeater, M.dry, P.Ttub));}});
 S.push_back({"F04-airlock-persistent", 3*H_, 0.1, []{P.Ttub=25;P.airlockS=1e6;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.lockoutSeen&&M.maxHeater<44, F("lockout=%d max heater %.1f dry %.1fs", M.lockoutSeen, M.maxHeater, M.dry));}});
 S.push_back({"F05-filter-clogging-over-3-days", 3*24*H_, 0.2, []{P.Ttub=36;P.profile=true;P.dayAmb=20;P.nightAmb=10;P.sunPeak=200;P.startMin=0;},
  {AT(1, heatSolar(38)), AT(24*H_, P.flowFactor=0.5), AT(36*H_, P.flowFactor=0.3), AT(48*H_, P.flowFactor=0.15), AT(60*H_, P.flowFactor=0.08)},
  []{return expect(cleanInv()&&M.maxHeater<44, F("INFO detected=%d at %.1fh; max heater %.1f; lockout=%d; min tub %.2f", M.firstFaultCode, M.firstFaultT/3600, M.maxHeater, M.lockoutSeen, M.minTub));}});

 // ============================ component failures
 S.push_back({"C01-ssr-fails-shorted", 3*H_, 0.1, []{P.Ttub=34;P.ssrShort=true;},
  {AT(1, heatManual(38))},
  []{return expect(seen(FaultCode::HEATER_SSR_SUSPECT)&&M.lockoutSeen, F("SSR suspect=%d at %.0fs; lockout %d; max tub %.2f", seen(FaultCode::HEATER_SSR_SUSPECT), M.faultFirstT[(int)FaultCode::HEATER_SSR_SUSPECT], M.lockoutSeen, M.maxTub));}});
 S.push_back({"C02-safety-contactor-welded", 6*H_, 0.1, []{P.Ttub=34;P.safetyWeld=true;},
  {AT(1, heatManual(38))},
  []{return expect(M.maxTub<=38.8, F("INFO undetectable without feedback: heats normally, max %.2f; heatNoLow=%ld", M.maxTub, M.heatNoLow));}});
 S.push_back({"C03-welded-contactor-plus-firmware-hang", 24*H_, 0.2, []{P.Ttub=37;P.safetyWeld=true;},
  {AT(1, heatManual(38)), AT(1, cmd("setpoint 40")), AT(20*MIN_, P.hung=true)},
  []{return expect(M.maxTub<=44.5, F("high-limit feeds SSR too: max tub %.2f, high-limit tripped %d", M.maxTub, P.hlTripped));}});
 S.push_back({"C04-ssr-short-and-weld-double-fault", 8*H_, 0.1, []{P.Ttub=37;P.ssrShort=true;P.safetyWeld=true;},
  {AT(1, heatManual(37))},
  []{return expect(P.fuseBlown&&M.maxTub<=40, F("thermal fuse blown %d; max tub %.2f heater body %.1f (heatNoLow=%ld expected: hardware double fault)", P.fuseBlown, M.maxTub, M.maxHeater, M.heatNoLow));}});
 S.push_back({"C05-low-spare-pole2-open", 2*H_, 0.1, []{P.Ttub=34;P.lowPole2Open=true;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.heaterOn==0&&seen(FaultCode::HEATER_CONTACTOR_FAULT), F("heater never energised; no-response fault at %.0fs", M.faultFirstT[(int)FaultCode::HEATER_CONTACTOR_FAULT]));}});
 S.push_back({"C06-low-welded-user-wants-high", 1*H_, 0.1, []{P.Ttub=38;P.lowWeld=true;},
  {AT(10, endPriming()), AT(5*MIN_, cmd("low")), AT(5*MIN_+3, cmd("high"))},
  []{return expect(M.highOn==0, F("INFO HIGH silently unavailable (no feedback); fault %d", M.firstFaultCode));}});
 S.push_back({"C07-heater-element-open", 2*H_, 0.1, []{P.Ttub=34;P.heaterOpen=true;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&seen(FaultCode::HEATER_CONTACTOR_FAULT), F("element open: no-response fault at %.0fs", M.faultFirstT[(int)FaultCode::HEATER_CONTACTOR_FAULT]));}});

 // ============================ controller
 S.push_back({"H01-firmware-hang-heating-24h", 24*H_, 0.2, []{P.Ttub=36;},
  {AT(1, heatManual(38)), AT(1, cmd("setpoint 40")), AT(30*MIN_, P.hung=true)},
  []{return expect(M.maxTub<=44.5, F("outputs frozen on: high-limit %d, max tub %.2f, heater body %.1f", P.hlTripped, M.maxTub, M.maxHeater));}});

 // ============================ environment
 S.push_back({"E01-freeze-heat-off", 12*H_, 0.2, []{P.Ttub=6;P.ambient=-5;P.bay=-2;},
  {},
  []{return expect(cleanInv()&&M.freezeSeen&&M.minTub>4.0, F("freeze runs %d; min tub %.2f; LOW %.1fh", M.freezeSeen, M.minTub, M.lowOn/3600));}});
 S.push_back({"E02-freeze-sensorB-failed-high", 12*H_, 0.2, []{P.Ttub=6;P.ambient=-5;P.bay=-2;P.sB={3,30};},
  {},
  []{return expect(M.freezeSeen&&M.minTub>4.0, F("freeze via A despite B fault: %d; min tub %.2f; faults %d", M.freezeSeen, M.minTub, M.firstFaultCode));}});
 S.push_back({"E03-freeze-both-sensors-open", 12*H_, 0.2, []{P.Ttub=6;P.ambient=-5;P.bay=-2;P.sA={1,0};P.sB={1,0};},
  {},
  []{return expect(M.lowOn>0, F("blind circulation LOW %.1fh; min tub %.2f", M.lowOn/3600, M.minTub));}});
 S.push_back({"E04-no-clock-solar-mode", 24*H_, 0.2, []{P.Ttub=36;P.ambient=12;P.clockValid=false;},
  {AT(1, heatSolar(38))},
  []{return expect(cleanInv()&&M.minTub>=33.5&&M.maxTub<=36.6, F("no time: floor only; min %.2f max %.2f; LOW %.1fh", M.minTub, M.maxTub, M.lowOn/3600));}});

 // ============================ randomized fuzz: random commands, glitches and flow dips
 for(int seed=1; seed<=60; ++seed){
  S.push_back({F("Z%02d-fuzz", seed), 6*H_, 0.1, [seed]{rng.seed(seed); P.Ttub=30+seed%9; P.startMin=(seed*137)%1440;},
   {AT(1, heatManual(38))},
   []{bool ok=cleanInv()&&M.maxTub<=40.6&&M.maxHeater<44;   // dry_s is cumulative over several injected flow losses
      return expect(ok, F("max tub %.2f dry %.1fs max heater %.1f faults %d; %s", M.maxTub, M.dry, M.maxHeater, M.firstFaultCode, inv().c_str()));}});
  // random events every 1-15 min
  std::mt19937 g(seed*7919); std::uniform_real_distribution<double> U(0,1);
  const char* cmds[]={"low","high","off","light","light on","light off","lightprog 2","ready","standby","heatmode manual","heatmode solar","heatmode off","setpoint 40","setpoint 36","afteruse","primeend","resetfault confirm"};
  double t=60;
  while(t<6*H_){ t+=60+U(g)*840; double r=U(g);
   if(r<0.55){ int i=(int)(U(g)*17); std::string c=cmds[i]; S.back().acts.push_back({t,[c]{cmd(c.c_str());}}); }
   else if(r<0.70){ double ff=U(g)<0.5?0.2:0.0, d=5+U(g)*120; S.back().acts.push_back({t,[ff]{P.flowFactor=ff;}}); S.back().acts.push_back({t+d,[]{P.flowFactor=1.0;}}); }
   else if(r<0.82){ int m=1+(int)(U(g)*2); double d=1+U(g)*20; bool a=U(g)<0.5; S.back().acts.push_back({t,[m,a]{(a?P.sA:P.sB)={m,0};}}); S.back().acts.push_back({t+d,[a]{(a?P.sA:P.sB)={0,0};}}); }
   else if(r<0.90){ double d=0.2+U(g)*4; S.back().acts.push_back({t,[d]{P.v24=14;}}); S.back().acts.push_back({t+d,[]{P.v24=24;}}); }
   else { S.back().acts.push_back({t,[]{for(int i=0;i<8;i++){fakeMs+=150;cmd(i%2?"high":"low");}}}); }
  }
  std::sort(S.back().acts.begin(), S.back().acts.end(), [](const Act&a,const Act&b){return a.t<b.t;});
 }
}
