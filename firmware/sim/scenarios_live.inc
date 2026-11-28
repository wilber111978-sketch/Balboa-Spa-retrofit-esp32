// Live M7 commissioning-profile scenarios (build with -DFWSIM_LIVE_M7, plus -DFWSIM_NORMAL for L05).
#undef AT
#define AT(t, ...) Act{(double)(t), []{ __VA_ARGS__; }}
static bool cleanInv(){ return !M.lowHigh && !M.heatNoLow && !M.heatHigh && !M.chatter && M.uvNoPump < 0.5 && M.heatPriming == 0; }
static std::string inv(){ return F("inv lowHigh=%ld heatNoLow=%ld heatHigh=%ld chatter=%ld uvNoPump=%.1fs heatPriming=%.1fs", M.lowHigh, M.heatNoLow, M.heatHigh, M.chatter, M.uvNoPump, M.heatPriming); }
static void heatManual(double sp){ cmd("heatmode manual"); cmd(F("setpoint %.1f", sp).c_str()); cmd("ready"); }
void defineScenarios(){
#ifndef FWSIM_NORMAL
 S.push_back({"L01-good-flow-pulse-passes", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.livePassed&&!M.firstFaultCode&&M.heaterOn>=44&&M.heaterOn<=46.5,
     F("qualification passed=%d at %.0fs; heater on %.1fs (pulse only); max heater body %.2f; faults %d; %s", M.livePassed, M.livePassT, M.heaterOn, M.maxHeater, M.firstFaultCode, inv().c_str()));}});
 S.push_back({"L02-no-flow-pulse-faults", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.flowFactor=0.0;},
  {AT(1, heatManual(38))},
  []{return expect(!M.livePassed&&M.firstFaultCode!=0&&M.heaterOn<=46.5&&M.maxHeater<44,
     F("no flow: passed=%d first fault %d at %.0fs; heater on %.1fs; max heater body %.2f", M.livePassed, M.firstFaultCode, M.firstFaultT, M.heaterOn, M.maxHeater));}});
 S.push_back({"L03-weak-flow-pulse-faults", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.flowFactor=0.15;},
  {AT(1, heatManual(38))},
  []{return expect(!M.livePassed&&M.heaterOn<=46.5&&M.maxHeater<44,
     F("15%% flow: passed=%d first fault %d; heater on %.1fs; max heater body %.2f", M.livePassed, M.firstFaultCode, M.heaterOn, M.maxHeater));}});
 S.push_back({"L04-ssr-shorted-caught-by-proof", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.ssrShort=true;},
  {AT(1, heatManual(38))},
  []{return expect(seen(FaultCode::HEATER_SSR_SUSPECT)&&!M.livePassed&&M.maxHeater<44,
     F("SSR short: SSR-suspect fault=%d at %.0fs; heater on %.1fs; max heater body %.2f", seen(FaultCode::HEATER_SSR_SUSPECT), M.faultFirstT[15], M.heaterOn, M.maxHeater));}});
 S.push_back({"L06-element-open-no-response", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.heaterOpen=true;},
  {AT(1, heatManual(38))},
  []{return expect(strstr(faultDetail,"no heat response")!=nullptr&&!M.livePassed&&M.firstFaultCode!=0,
     F("element open: passed=%d first fault %d at %.0fs", M.livePassed, M.firstFaultCode, M.firstFaultT));}});
 S.push_back({"L07-jets-high-blocks-heater", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;},
  {AT(1, heatManual(38)), AT(60, cmd("high"))},
  []{return expect(cleanInv(),F("jets on HIGH during qualification: %s; heater on %.1fs", inv().c_str(), M.heaterOn));}});
#else
 S.push_back({"L05-normal-heating-after-pass", 4*H_, 0.1, []{P.Ttub=34;P.heaterW=1500;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.livePassed&&!M.firstFaultCode&&M.maxTub>=37.85&&M.maxTub<38.6,
     F("passed at %.0fs; heated 34->max %.2f final %.2f; heater %.2fh; %s", M.livePassT, M.maxTub, P.Ttub, M.heaterOn/3600, inv().c_str()));}});
 // V0.9.3: owner's spa as measured 2026-11-26: 1.5 kW, ~1500 L, high LOW-speed flow so the
 // heater opens only ~0.2 C between the probes. The old fixed 0.25 C check faulted here.
 S.push_back({"L08-weak-dT-real-spa-no-false-fault", 2*H_, 0.1, []{P.Ttub=20.5;P.heaterW=1500;P.tubKg=1500;P.flowLow=1.8;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.livePassed&&!M.firstFaultCode&&M.heaterOn>3000&&P.Ttub>21.0,
     F("weak dT: passed=%d faults %d; heater %.0fs; tub 20.5->%.2f; %s", M.livePassed, M.firstFaultCode, M.heaterOn, P.Ttub, inv().c_str()));}});
 // V0.9.3 ride-through: a controller loop stall (2-5 s; longer resets via the 5 s watchdog)
 // pauses the heater but must not shut the spa down, during normal heating or the test.
 S.push_back({"L10-3s-stall-while-heating-no-fault", 2*H_, 0.1, []{P.Ttub=24;P.heaterW=1500;P.tubKg=400;P.flowLow=1.8;},
  {AT(1, heatManual(38)), AT(3600, P.hung=true), AT(3603, P.hung=false)},
  []{return expect(M.livePassed&&!M.firstFaultCode&&P.Ttub>28,
     F("3 s stall at 3600 s: faults %d; tub 24->%.2f", M.firstFaultCode, P.Ttub));}});
 S.push_back({"L11-3s-stall-during-heater-test-retries", 1*H_, 0.1, []{P.Ttub=24;P.heaterW=1500;P.tubKg=400;P.flowLow=1.8;},
  {AT(1, heatManual(38)), AT(200, P.hung=true), AT(203, P.hung=false)},
  []{return expect(M.livePassed&&!M.firstFaultCode&&M.livePassT>203,
     F("3 s stall in the pulse: passed=%d at %.0fs, faults %d", M.livePassed, M.livePassT, M.firstFaultCode));}});
 // Mid-run element failure: the A-B shift was already seen at SSR start, so the 45 min
 // tub-rise backup catches it (a dead element is a no-heat failure, not a hazard).
 S.push_back({"L09-element-fails-after-pass", 2*H_, 0.1, []{P.Ttub=20.5;P.heaterW=1500;P.tubKg=1500;P.flowLow=1.8;},
  {AT(1, heatManual(38)), AT(1200, P.heaterOpen=true)},
  []{return expect(M.livePassed&&seen(FaultCode::HEATER_CONTACTOR_FAULT)&&M.faultFirstT[14]>1200&&M.faultFirstT[14]<1200+45*60+60,
     F("element open mid-run at 1200s: caught by 45 min tub-rise backup: fault=%d at %.0fs", seen(FaultCode::HEATER_CONTACTOR_FAULT), M.faultFirstT[14]));}});
#endif
}
