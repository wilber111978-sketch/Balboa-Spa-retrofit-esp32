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
  []{return expect(cleanInv()&&M.livePassed&&!M.firstFaultCode&&M.heaterOn>=29&&M.heaterOn<=31.5,
     F("qualification passed=%d at %.0fs; heater on %.1fs (pulse only); max heater body %.2f; faults %d; %s", M.livePassed, M.livePassT, M.heaterOn, M.maxHeater, M.firstFaultCode, inv().c_str()));}});
 S.push_back({"L02-no-flow-pulse-faults", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.flowFactor=0.0;},
  {AT(1, heatManual(38))},
  []{return expect(!M.livePassed&&M.firstFaultCode!=0&&M.heaterOn<=31.5&&M.maxHeater<44,
     F("no flow: passed=%d first fault %d at %.0fs; heater on %.1fs; max heater body %.2f", M.livePassed, M.firstFaultCode, M.firstFaultT, M.heaterOn, M.maxHeater));}});
 S.push_back({"L03-weak-flow-pulse-faults", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.flowFactor=0.15;},
  {AT(1, heatManual(38))},
  []{return expect(!M.livePassed&&M.heaterOn<=31.5&&M.maxHeater<44,
     F("15%% flow: passed=%d first fault %d; heater on %.1fs; max heater body %.2f", M.livePassed, M.firstFaultCode, M.heaterOn, M.maxHeater));}});
 S.push_back({"L04-ssr-shorted-caught-by-proof", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.ssrShort=true;},
  {AT(1, heatManual(38))},
  []{return expect(seen(FaultCode::HEATER_SSR_SUSPECT)&&!M.livePassed&&M.maxHeater<44,
     F("SSR short: SSR-suspect fault=%d at %.0fs; heater on %.1fs; max heater body %.2f", seen(FaultCode::HEATER_SSR_SUSPECT), M.faultFirstT[15], M.heaterOn, M.maxHeater));}});
 S.push_back({"L06-element-open-no-response", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;P.heaterOpen=true;},
  {AT(1, heatManual(38))},
  []{return expect(!M.livePassed&&M.firstFaultCode!=0,
     F("element open: passed=%d first fault %d at %.0fs", M.livePassed, M.firstFaultCode, M.firstFaultT));}});
 S.push_back({"L07-jets-high-blocks-heater", 30*MIN_, 0.1, []{P.Ttub=34;P.heaterW=1500;},
  {AT(1, heatManual(38)), AT(60, cmd("high"))},
  []{return expect(cleanInv(),F("jets on HIGH during qualification: %s; heater on %.1fs", inv().c_str(), M.heaterOn));}});
#else
 S.push_back({"L05-normal-heating-after-pass", 4*H_, 0.1, []{P.Ttub=34;P.heaterW=1500;},
  {AT(1, heatManual(38))},
  []{return expect(cleanInv()&&M.livePassed&&!M.firstFaultCode&&M.maxTub>=37.85&&M.maxTub<38.6,
     F("passed at %.0fs; heated 34->max %.2f final %.2f; heater %.2fh; %s", M.livePassT, M.maxTub, P.Ttub, M.heaterOn/3600, inv().c_str()));}});
#endif
}
