// Firmware-in-the-loop plant simulation for V0.9.
// Compiles the REAL SpaController.h (Main build, onboard ADC path) against a
// physical spa model. HOST ONLY: SPA_SIM_COMMISSIONED refuses ESP32 builds.
#define SPA_BENCH_DEMO 0
#define SPA_TEMP_ADC_ADS1115 0
#define SPA_SIM_COMMISSIONED 1
#define SPA_PIN_HIGH_LIMIT_OK 16
#include "../libraries/SpaController/src/SpaController.h"
#include <cstdio>
#include <functional>
#include <random>
#include <unistd.h>

static const double CW = 4186.0;
static const double TBL[][2] = {{-23,320e3},{-13,184e3},{0,95e3},{11.5,55e3},{18.5,40e3},{25,30e3},{29,25.5e3},{34.5,20.2e3},{37,18.1e3},{38.5,17.2e3},{40,16.2e3},{41,15.4e3},{42.5,14.7e3},{43.5,14.1e3},{44.5,13.6e3},{45,13.2e3},{46,12.7e3},{47.5,12.1e3},{48.5,11.7e3},{49.5,11.1e3},{52.5,10e3},{55,9e3},{62.5,7e3},{72,5e3},{88,3e3},{110.5,1.5e3}};
static double tToR(double t){const int n=sizeof(TBL)/sizeof(TBL[0]);if(t<=TBL[0][0])return TBL[0][1];if(t>=TBL[n-1][0])return TBL[n-1][1];
 for(int i=0;i<n-1;i++)if(t>=TBL[i][0]&&t<=TBL[i+1][0]){double f=(t-TBL[i][0])/(TBL[i+1][0]-TBL[i][0]);return exp(log(TBL[i][1])+f*(log(TBL[i+1][1])-log(TBL[i][1])));}return 30e3;}

// ---------------------------------------------------------------- plant (POD so it survives a simulated reboot)
struct SensorFault { int mode; double value; };  // 0 ok,1 open,2 short,3 stuck,4 offset,5 intermittent-open
struct Plant {
  double Ttub=36, Ta=36, Tb=36, ambient=20, bay=23, solarW=0, tubKg=800, ua=6, Ch=7000, heaterW=2000;
  double flowLow=0.67, flowHigh=2.0, flowFactor=1.0, airlockS=0;
  double hlTrip=44.0; bool hlTripped=false; double fuseTemp=72.0; bool fuseBlown=false;
  double v24=24; bool mains=true; bool hung=false;
  bool ssrShort=false, safetyWeld=false, lowWeld=false, lowPole1Open=false, lowPole2Open=false, heaterOpen=false, swapAB=false;
  SensorFault sA{0,0}, sB{0,0};
  double adcGain=1.0, adcOffsetMv=0, adcNoiseMv=1.5;
  int lightN=7; double lightStepMax=1.5, lightResetOff=8.0; bool lightMemory=false; int lightProg=1; bool lightPowered=false; double lightOffSince=-100;
  double dayAmb=0, nightAmb=0, sunPeak=0; bool profile=false;
  int startMin=8*60; bool clockValid=true;
  bool LOWK=false, HIGHK=false, SAFETY=false, SSR=false, heater=false; double flow=0; int speed=0;
  double current=0;
};
struct Metrics {
  long lowHigh=0, heatNoLow=0, heatHigh=0, loadBreak=0, chatter=0;
  double uvNoPump=0, uvNoPumpSag=0, dry=0, heatPriming=0, over10A=0, maxAmps=0, maxTub=-99, minTub=99, maxHeater=-99;
  double heaterOn=0, lowOn=0, highOn=0, uvOn=0, maxDispErr=0, maxDispAgeShownMin=0, lightDesync=0;
  long pumpStarts=0, safetyOps=0, ssrOps=0;
  int firstFaultCode=0; double firstFaultT=-1; int faultsSeen[32]={}; double faultFirstT[32]={};
  bool lockoutSeen=false, waterHotSeen=false, mismatchSeen=false, freezeSeen=false, floorSeen=false, primingSeen=false, flowRetrySeen=false;
  double tStart=0; double tub0=0;
};
Plant P; Metrics M; std::mt19937 rng(7);
double simT=0;
uint32_t msOffset=0;
static uint32_t msAt(double t){return 100u+msOffset+(uint32_t)llround(t*1000.0);}

void lightStep(bool powered){
  if(powered&&!P.lightPowered){double off=simT-P.lightOffSince;if(off<=P.lightStepMax)P.lightProg=P.lightProg%P.lightN+1;else if(off>=P.lightResetOff&&!P.lightMemory)P.lightProg=1;}
  if(!powered&&P.lightPowered)P.lightOffSince=simT; P.lightPowered=powered;}

void plantStep(double dt){
  bool esp = P.mains;   // ESP32 powered from the same 24 V PSU
  // CH1/CH3 physical pins are permanently forced OFF in shipping code; the plant follows the
  // firmware's logical heater commands, which is what those relays carry once commissioned.
  bool ch1=esp&&spa.heaterSafetyContactor, ch2=esp&&pins[2], ch3=esp&&spa.heaterSSR, ch4=esp&&pins[42], uv=esp&&pins[45], li=esp&&pins[46];
  bool ok=P.mains&&P.v24>=18.0, prevLOW=P.LOWK, prevHIGH=P.HIGHK, prevHeat=P.heater, prevSafety=P.SAFETY, prevSSR=P.SSR;
  bool wantL=ch2&&ok&&!prevHIGH, wantH=ch4&&ok&&!prevLOW;
  if(wantL&&wantH&&!prevLOW&&!prevHIGH){M.chatter++;wantL=wantH=false;}
  P.LOWK=P.lowWeld||wantL; P.HIGHK=wantH&&!P.LOWK;
  bool hlOK=!P.hlTripped;                       // fix 1: high-limit feeds both 24 V heater branches
  P.SAFETY=P.safetyWeld||(ch1&&ok&&hlOK&&P.LOWK&&!P.lowPole1Open);
  P.SSR=P.ssrShort||(ch3&&ok&&hlOK&&P.LOWK&&!P.lowPole2Open);
  if(P.Tb>=P.fuseTemp)P.fuseBlown=true;
  P.heater=P.mains&&P.SAFETY&&P.SSR&&!P.heaterOpen&&!P.fuseBlown;
  pins[16]=hlOK;                                  // high-limit status input (GPIO16)
  if(prevLOW&&!P.LOWK&&prevHeat&&P.mains&&ok)M.loadBreak++;
  if(P.LOWK&&!prevLOW)M.pumpStarts++; if(P.HIGHK&&!prevHIGH)M.pumpStarts++;
  if(P.SAFETY!=prevSafety)M.safetyOps++; if(P.SSR!=prevSSR)M.ssrOps++;
  lightStep(li&&P.mains);
  int speed=P.HIGHK?2:P.LOWK?1:0; if(!P.mains)speed=0; P.speed=speed;
  double base=speed==2?P.flowHigh:speed==1?P.flowLow:0;
  if(P.airlockS>0&&speed){P.airlockS-=dt*(speed==2?3:1);base=0;}
  P.flow=base*P.flowFactor;
  if(P.profile){double hr=fmod(P.startMin/60.0+simT/3600.0,24.0);
    P.ambient=(hr>=8&&hr<=20)?P.nightAmb+(P.dayAmb-P.nightAmb)*fmax(0,sin((hr-8)/12*M_PI)):P.nightAmb;
    P.solarW=(hr>=9&&hr<=17)?P.sunPeak*fmax(0,sin((hr-9)/8*M_PI)):0; P.bay=P.ambient+3;}
  double Pw=P.heater?P.heaterW:0, m=P.flow, dQb;
  if(m>0){P.Ta+=(P.Ttub-P.Ta)*fmin(1.0,dt/5.0);dQb=Pw+m*CW*(P.Ta-P.Tb)-2.0*(P.Tb-P.bay);}
  else{P.Ta+=((P.bay-P.Ta)/1800.0+(P.Tb-P.Ta)/900.0)*dt;dQb=Pw-3.0*(P.Tb-P.bay);}
  P.Tb+=dQb*dt/P.Ch;
  double pumpHeat=speed==2?600:speed==1?150:0;
  double gain=(m>0?m*CW*(P.Tb-P.Ta):0)+pumpHeat-P.ua*(P.Ttub-P.ambient)+P.solarW;
  P.Ttub+=gain*dt/(P.tubKg*CW);
  if(P.Tb>=P.hlTrip)P.hlTripped=true;
  double a=P.mains?0.4:0; if(P.heater)a+=8.33; if(speed==1)a+=2.0; if(speed==2)a+=6.5; if(uv)a+=0.3; if(P.lightPowered)a+=0.4; P.current=a;
  // sensors -> onboard ADC millivolts
  std::normal_distribution<double> nz(0,P.adcNoiseMv);
  auto mv=[&](SensorFault f,double temp)->uint32_t{
    if(f.mode==1)return 3100; if(f.mode==2)return 5; if(f.mode==5&&fmod(simT,600.0)<1.0)return 3100;
    if(f.mode==3)temp=f.value; if(f.mode==4)temp+=f.value;
    double r=tToR(temp), v=3300.0*r/(r+30000.0)*P.adcGain+P.adcOffsetMv+nz(rng);
    return (uint32_t)fmax(0,fmin(3100,v));};
  double tA=P.swapAB?P.Tb:P.Ta, tB=P.swapAB?P.Ta:P.Tb;
  fakeMv[8]=mv(P.sA,tA); fakeMv[9]=mv(P.sB,tB);
}

void invariants(double dt){
  if(P.LOWK&&P.HIGHK)M.lowHigh++; if(P.heater&&!P.LOWK)M.heatNoLow++; if(P.heater&&P.HIGHK)M.heatHigh++;
  bool uv=P.mains&&pins[45]; if(uv&&P.speed==0){ if(P.v24>=18)M.uvNoPump+=dt; else M.uvNoPumpSag+=dt; } if(uv)M.uvOn+=dt;
  if(P.heater&&P.flow<=0.01)M.dry+=dt; if(P.heater)M.heaterOn+=dt; if(P.heater&&P.mains&&primingActive)M.heatPriming+=dt;
  if(P.current>10.0)M.over10A+=dt; M.maxAmps=fmax(M.maxAmps,P.current);
  if(P.speed==1)M.lowOn+=dt; if(P.speed==2)M.highOn+=dt;
  M.maxTub=fmax(M.maxTub,P.Ttub); M.minTub=fmin(M.minTub,P.Ttub); M.maxHeater=fmax(M.maxHeater,P.Tb);
  if(!P.mains)return;
  int fc=(int)spa.fault; if(fc>0&&fc<32&&!M.faultsSeen[fc]){M.faultsSeen[fc]=1;M.faultFirstT[fc]=simT;if(!M.firstFaultCode){M.firstFaultCode=fc;M.firstFaultT=simT;}}
  M.lockoutSeen|=spa.lockedOut; M.waterHotSeen|=spa.waterHot; M.mismatchSeen|=spa.calibrationInhibit; M.freezeSeen|=freezeActive;
  M.floorSeen|=floorHeatNeeded; M.primingSeen|=primingActive; M.flowRetrySeen|=flowRetryPending;
  if(circulatingWaterValid){double age=(millis()-lastCirculatingWaterMs)/60000.0;
    if(age<5)M.maxDispErr=fmax(M.maxDispErr,fabs(lastCirculatingWaterC-P.Ttub));}
  if(lightOn&&lightQueueLen==0&&P.lightPowered&&P.lightProg!=lightProgram)M.lightDesync+=dt;
}

// ---------------------------------------------------------------- scenario plumbing
struct Act { double t; std::function<void()> f; };
struct Scen { std::string name; double dur, dt; std::function<void()> init; std::vector<Act> acts; std::function<std::string()> verdict; };
std::vector<Scen> S;
double outageEnd=-1;
std::string scenName; std::string savePath;

void saveAndReboot(const char*self){
  FILE*f=fopen(savePath.c_str(),"wb"); fwrite(&simT,sizeof simT,1,f); fwrite(&outageEnd,sizeof outageEnd,1,f);
  fwrite(&P,sizeof P,1,f); fwrite(&M,sizeof M,1,f);
  size_t n=Preferences::db.size(); fwrite(&n,sizeof n,1,f);
  for(auto&kv:Preferences::db){size_t l=kv.first.size();fwrite(&l,sizeof l,1,f);fwrite(kv.first.data(),1,l,f);fwrite(&kv.second,sizeof kv.second,1,f);}
  n=Preferences::blobs.size(); fwrite(&n,sizeof n,1,f);
  for(auto&kv:Preferences::blobs){size_t l=kv.first.size();fwrite(&l,sizeof l,1,f);fwrite(kv.first.data(),1,l,f);size_t b=kv.second.size();fwrite(&b,sizeof b,1,f);fwrite(kv.second.data(),1,b,f);}
  fclose(f); fflush(stdout);
  execl(self,self,scenName.c_str(),"--resume",savePath.c_str(),(char*)nullptr); perror("exec"); exit(3);
}
bool loadResume(const char*path){
  FILE*f=fopen(path,"rb"); if(!f)return false; fread(&simT,sizeof simT,1,f); fread(&outageEnd,sizeof outageEnd,1,f);
  fread(&P,sizeof P,1,f); fread(&M,sizeof M,1,f); size_t n; fread(&n,sizeof n,1,f);
  for(size_t i=0;i<n;i++){size_t l;fread(&l,sizeof l,1,f);std::string k(l,' ');fread(&k[0],1,l,f);double v;fread(&v,sizeof v,1,f);Preferences::db[k]=v;}
  fread(&n,sizeof n,1,f);
  for(size_t i=0;i<n;i++){size_t l;fread(&l,sizeof l,1,f);std::string k(l,' ');fread(&k[0],1,l,f);size_t b;fread(&b,sizeof b,1,f);std::vector<uint8_t> v(b);fread(v.data(),1,b,f);Preferences::blobs[k]=v;}
  fclose(f); return true;
}
void setClock(){ fakeClockValid=P.clockValid; int mins=P.startMin+(int)(simT/60.0); fakeMinute=mins%1440; fakeDay=10+mins/1440; }
void cmd(const char*c){ executeCommand(String(c)); }
#define AT(t, ...) Act{(double)(t), [&]{ __VA_ARGS__; }}
static const double MIN_=60, H_=3600;
std::string F(const char*fmt,...){char b[512];va_list a;va_start(a,fmt);vsnprintf(b,sizeof b,fmt,a);va_end(a);return b;}
bool seen(FaultCode c){return M.faultsSeen[(int)c];}
std::string expect(bool ok,const std::string&why){return std::string(ok?"PASS":"FAIL")+": "+why;}

void defineScenarios();

int main(int argc,char**argv){
  if(argc<2){fprintf(stderr,"usage: fw_sim <scenario>|--list\n");return 2;}
  defineScenarios();
  scenName=argv[1];
  if(scenName=="--list"){for(auto&s:S)printf("%s\n",s.name.c_str());return 0;}
  Scen*sc=nullptr; for(auto&s:S)if(s.name==scenName)sc=&s; if(!sc){fprintf(stderr,"unknown %s\n",argv[1]);return 2;}
  savePath="/tmp/fwsim_"+scenName+"_"+std::to_string(getpid())+".bin";
  bool resumed=false;
  if(argc>=4&&std::string(argv[2])=="--resume"){resumed=loadResume(argv[3]);unlink(argv[3]);savePath=argv[3];}
  if(!resumed){sc->init(); P.Ta=P.Tb=P.Ttub; M.tub0=P.Ttub;}
  fakeResetReason=ESP_RST_POWERON;
  if(resumed){ // finish the outage with ESP32 unpowered
    for(;simT<outageEnd;simT+=sc->dt){for(int p=0;p<64;p++)pins[p]=0;plantStep(sc->dt);invariants(sc->dt);}
    P.mains=true; outageEnd=-1;
  }
  { for(int p=0;p<64;p++)pins[p]=0; double keep=P.Ttub; plantStep(1e-6); P.Ttub=keep; }  // sensor voltages valid before boot
  fakeMs=msAt(simT); setClock(); setup();
  if(fakeMs>msAt(simT)) msOffset+=fakeMs-msAt(simT);   // boot delays consume real time
  size_t ai=0; while(ai<sc->acts.size()&&sc->acts[ai].t<simT-1e-9)ai++;
  const double dt=sc->dt;
  for(;simT<sc->dur;simT+=dt){
    while(ai<sc->acts.size()&&sc->acts[ai].t<=simT+1e-9){sc->acts[ai].f();ai++;}
    if(!P.mains){ // power cut: ESP32 off; reboot as a fresh process when power returns
      if(outageEnd<0){fprintf(stderr,"power cut without restore time\n");return 2;}
      for(int p=0;p<64;p++)pins[p]=0;
      saveAndReboot(argv[0]);
    }
    // Main has no heater-qualification procedure yet (V0.8: NOT implemented); the simulator grants it
    // exactly as the demo build does: from the high-limit status input.
    if(!P.hung){fakeMs=msAt(simT);setClock();spa.heatQualificationPassed=pins[16]!=0;loop();}
    plantStep(dt); invariants(dt);
    if(getenv("SIM_PROBE")&&fmod(simT,600.0)<dt*0.5)fprintf(stderr,"t=%.0f ms=%u prime=%d permit=%d fault=%d lock=%d hl=%d A=%.2f B=%.2f stA=%d stB=%d hm=%d ready=%d heatNeed=%d contactor=%d ssr=%d act=%d\n",simT,(unsigned)millis(),primingActive,heaterPermitted(),(int)spa.fault,spa.lockedOut,hardwareHighLimitOK(),tempA.filteredC,tempB.filteredC,(int)tempA.status,(int)tempB.status,heatMode,readyMode,heatPumpNeeded,spa.heaterSafetyContactor,spa.heaterSSR,(int)spa.actualPump);
    if(getenv("SIM_PROBE")&&fmod(simT,600.0)<dt*0.5)fprintf(stderr,"   req=%d lowStart=%u qual=%d healthy=%d diff=%.2f thermo=%d sched=%d commissioned=%d svc=%d stor=%d\n",(int)spa.requestedPump,(unsigned)spa.pumpLowStartedMs,heaterQualificationReady(),temperatureSystemHealthy(),spa.tempDifference,spa.thermostatHeating,schedulePermitsHeating(),HEATER_OUTPUTS_COMMISSIONED,serviceMode,storageHealthy);
  }
  std::string v=sc->verdict();
  std::string faults; for(int i=1;i<32;i++)if(M.faultsSeen[i])faults+=F("%s%d@%.0fs",faults.empty()?"":",",i,M.faultFirstT[i]);
  printf("{\"scenario\":\"%s\",\"verdict\":\"%s\",\"inv\":{\"lowHigh\":%ld,\"heatNoLow\":%ld,\"heatHigh\":%ld,\"loadBreak\":%ld,\"chatter\":%ld,\"uvNoPump_s\":%.1f,\"uvDuring24VSag_s\":%.1f,\"heatDuringPriming_s\":%.1f},"
         "\"tub\":[%.2f,%.2f,%.2f,%.2f],\"maxHeater\":%.1f,\"dry_s\":%.1f,\"heaterOn_h\":%.2f,\"low_h\":%.2f,\"high_min\":%.1f,\"uv_h\":%.2f,\"pumpStarts\":%ld,\"safetyOps\":%ld,\"ssrOps\":%ld,"
         "\"maxAmps\":%.2f,\"over10A_min\":%.1f,\"maxDispErr\":%.2f,\"lightDesync_s\":%.1f,\"faults\":\"%s\",\"lockout\":%s,\"flags\":\"%s%s%s%s%s%s%s\",\"hl\":%s,\"fuse\":%s}\n",
    scenName.c_str(),v.c_str(),M.lowHigh,M.heatNoLow,M.heatHigh,M.loadBreak,M.chatter,M.uvNoPump,M.uvNoPumpSag,M.heatPriming,
    M.tub0,P.Ttub,M.minTub,M.maxTub,M.maxHeater,M.dry,M.heaterOn/3600,M.lowOn/3600,M.highOn/60,M.uvOn/3600,M.pumpStarts,M.safetyOps,M.ssrOps,
    M.maxAmps,M.over10A/60,M.maxDispErr,M.lightDesync,faults.c_str(),M.lockoutSeen?"true":"false",
    M.primingSeen?"P":"",M.freezeSeen?"F":"",M.floorSeen?"E":"",M.waterHotSeen?"H":"",M.mismatchSeen?"M":"",M.flowRetrySeen?"R":"",spa.lockedOut?"L":"",
    P.hlTripped?"true":"false",P.fuseBlown?"true":"false");
  return v.rfind("PASS",0)==0?0:1;
}
#include "scenarios.inc"
