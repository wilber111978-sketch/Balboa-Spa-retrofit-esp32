#pragma once
#include <LiveWebServer.h>
// Include the frozen baseline once, substituting only its entry-point/server
// symbols. No sensor, interlock, persistence, scheduling or relay code is forked.
#define BoundedWebServer LiveWebServer
#define setup spa062Setup
#define loop spa062Loop
#include <SpaController.h>
#undef loop
#undef setup
#undef BoundedWebServer
#include <ArduinoJson.h>
#include <set>
#include "WebAssets.h"

struct LiveFaultInfo { const char *id,*panel,*title,*trigger,*cause,*check; };
LiveFaultInfo liveFaultInfo(FaultCode code) {
    switch(code) {
    case FaultCode::TEMP_A_OPEN: case FaultCode::TEMP_A_SHORT: case FaultCode::TEMP_A_INVALID:
        return {"SENSOR_A_RANGE","Sn","Sensor A reading invalid","Sensor A failed voltage or temperature validation.","Sensor, connector or input circuit fault.","Isolate power and inspect Sensor A wiring and calibration."};
    case FaultCode::TEMP_B_OPEN: case FaultCode::TEMP_B_SHORT: case FaultCode::TEMP_B_INVALID:
        return {"SENSOR_B_RANGE","Sn","Sensor B reading invalid","Sensor B failed voltage or temperature validation.","Sensor, connector or input circuit fault.","Isolate power and inspect Sensor B wiring and calibration."};
    case FaultCode::TEMP_DISAGREE:return {"SENSOR_MISMATCH","Sn","Heater sensors disagree","The baseline disagreement timer tripped.","Probe mismatch or uneven circulation.","Check both probes and circulation before acknowledgement."};
    case FaultCode::OVERTEMP:return {"OVER_TEMP","OH","Overtemperature lockout","A raw or filtered probe reached the baseline temperature trip.","Unintended heat, circulation or sensor fault.","Keep heating isolated; check with an independent thermometer."};
    case FaultCode::RAPID_TEMP_RISE: case FaultCode::FLOW_SUSPECT:return {"NO_FLOW_SUSPECTED","dr","Thermal/flow anomaly","Baseline thermal-rate protection tripped.","Restricted flow, low water or probe problem.","Inspect circulation and the protective system before reset."};
    case FaultCode::SENSOR_STALE:return {"SENSOR_STALE","Sn","Sensor updates stale","A probe timestamp exceeded the baseline freshness limit.","Delayed control loop or acquisition failure.","Inspect acquisition timing and recorded events."};
    case FaultCode::WATCHDOG_RESET:return {"WATCHDOG_RECOVERY","","Watchdog/restart lockout","Watchdog or panic recovery requires acknowledgement.","Task stall or controller instability.","Inspect reset history; acknowledge then separately exit Service Mode."};
    case FaultCode::POWER_FAULT:return {"POWER_FAULT","","Power recovery lockout","Baseline brownout recovery latched a fault.","Supply instability.","Inspect control power before acknowledging."};
    case FaultCode::CONFIG_CORRUPT:return {"CONFIG_CORRUPT","","Settings integrity fault","Baseline settings validation or save verification failed.","Corrupt settings or failed NVS storage.","Repair stored settings deliberately; no automatic erase is provided."};
    case FaultCode::ADC_FAILURE:return {"ADC_FAILURE","Sn","ADC acquisition failure","ADC initialization or checked conversion failed.","ADC, I2C wiring or power fault.","Check the acquisition circuit and restart after repair."};
    case FaultCode::HIGH_LIMIT_OPEN:return {"HIGH_LIMIT_OPEN","","High-limit circuit open","The baseline high-limit input is not permissive.","Thermal protection or safety wiring fault.","Verify the independent protection before reset."};
    default:return {"CONTROLLER_FAULT","","Controller protection fault","A baseline protection latched a fault.","See the internal numeric code and baseline report.","Inspect the cause before requesting acknowledgement."};
    }
}
const char *livePump(PumpMode p){return p==PumpMode::LOW_SPEED?"LOW":p==PumpMode::HIGH_SPEED?"HIGH":"OFF";}
const char *liveMode(){return heatMode==1?"SOLAR_PRIORITY":heatMode==2?"MANUAL_LEGACY":"OFF";}
std::string liveTime(uint8_t hour){char out[6];snprintf(out,sizeof out,"%02u:00",(unsigned)(hour%24));return out;}
std::string liveJSON(JsonDocument &d){std::string out;serializeJson(d,out);return out;}
void liveSend(int code,JsonDocument &d){web.send(code,"application/json",String(liveJSON(d).c_str()));}
void liveReply(int code,const char *message,bool accepted=false){JsonDocument d;d["accepted"]=accepted;d["message"]=message;liveSend(code,d);}
template<class T> void liveNumber(T dest,float value){if(isfinite(value))dest=value;else dest=nullptr;}
bool liveInputsSafe(){return configurationValid&&storageHealthy&&(SPA_BENCH_DEMO||adcAvailable)&&
    tempA.status==SensorStatus::OK&&tempB.status==SensorStatus::OK&&isfinite(tempA.rawC)&&isfinite(tempB.rawC)&&
    isfinite(tempA.filteredC)&&isfinite(tempB.filteredC)&&millis()-tempA.lastUpdateMs<=SENSOR_STALE_MS&&millis()-tempB.lastUpdateMs<=SENSOR_STALE_MS&&
    fmax(tempA.rawC,tempB.rawC)<ABSOLUTE_TEMP_TRIP_C&&fmax(tempA.filteredC,tempB.filteredC)<ABSOLUTE_TEMP_TRIP_C&&
    fabs(tempA.filteredC-tempB.filteredC)<TEMP_DIFF_INHIBIT_C&&(!(SPA_BENCH_DEMO||PIN_HIGH_LIMIT_OK>=0)||hardwareHighLimitOK());}
struct LiveEvent { uint32_t at=0; FaultCode fault=FaultCode::NONE; std::string id,title; float a=NAN,b=NAN,delta=NAN,target=NAN; PumpMode pump=PumpMode::OFF;bool heater=false,uv=false; };
LiveEvent liveEvents[8];uint8_t liveEventHead=0,liveEventCount=0;FaultCode liveObservedFault=FaultCode::NONE;
uint32_t liveLastCommandMs=0;bool liveCommandSeen=false;
void liveRecord(const char *id,const char *title,FaultCode fault=FaultCode::NONE){
    LiveEvent &r=liveEvents[liveEventHead];r.at=millis();r.fault=fault;r.id=id;r.title=title;
    r.a=tempA.filteredC;r.b=tempB.filteredC;r.delta=spa.tempDifference;r.target=config.setpointC;r.pump=spa.actualPump;r.heater=spa.heaterSSR;r.uv=spa.uv;
    liveEventHead=(liveEventHead+1)%8;if(liveEventCount<8)++liveEventCount;
}
void liveObserveFault(){if(spa.fault!=liveObservedFault){liveObservedFault=spa.fault;if(spa.fault!=FaultCode::NONE){auto info=liveFaultInfo(spa.fault);liveRecord(info.id,info.title,spa.fault);}}}
void liveFaultJSON(JsonObject d,FaultCode code,const LiveEvent *record=nullptr){
    auto info=liveFaultInfo(code);d["panel_code"]=info.panel;d["id"]=info.id;d["title"]=info.title;d["internal_code"]=(uint16_t)code;
    d["trigger_condition"]=info.trigger;d["probable_causes"].to<JsonArray>().add(info.cause);d["recommended_checks"].to<JsonArray>().add(info.check);
    d["severity"]="FAULT";d["resettable"]=liveInputsSafe();d["retry_count"]=0;
    d["timestamp"]=nullptr;d["uptime_ms"]=record?record->at:millis();d["capture_phase"]="observed after baseline safety processing";
    auto snapshot=d["snapshot"].to<JsonObject>();liveNumber(snapshot["sensor_a_c"],record?record->a:tempA.filteredC);liveNumber(snapshot["sensor_b_c"],record?record->b:tempB.filteredC);
    liveNumber(snapshot["delta_t_c"],record?record->delta:spa.tempDifference);liveNumber(snapshot["setpoint_c"],record?record->target:config.setpointC);
    snapshot["pump"]=livePump(record?record->pump:spa.actualPump);snapshot["heater"]=record?record->heater:spa.heaterSSR;snapshot["uv"]=record?record->uv:spa.uv;
}
void liveState(){JsonDocument d;
    auto c=d["controller"].to<JsonObject>();c["online"]=true;c["uptime_s"]=millis()/1000;c["firmware"]=SPA_BENCH_DEMO?"0.6.3-live-demo":"0.6.3-live-main";c["build"]="live-api-1";c["reset_reason"]=resetReason;c["watchdog_ok"]=controlWatchdog!=nullptr;
    auto t=d["temperature"].to<JsonObject>();liveNumber(t["water_c"],spa.waterTemperature);t["target_c"]=config.setpointC;liveNumber(t["sensor_a_c"],tempA.filteredC);liveNumber(t["sensor_b_c"],tempB.filteredC);liveNumber(t["delta_t_c"],spa.tempDifference);liveNumber(t["rise_rate_c_min"],(tempA.rateCPerSec+tempB.rateCPerSec)*30);
    d["pump"]["requested"]=livePump(spa.requestedPump);d["pump"]["actual"]=livePump(spa.actualPump);
    d["heater"]["demand"]=spa.heaterDemand>0;d["heater"]["ssr_command"]=spa.heaterSSR;d["heater"]["safety_contactor"]=spa.heaterSafetyContactor;d["heater"]["allowed"]=false;
    d["heater"]["physical_outputs_inhibited"]=true;d["uv"]["on"]=spa.uv;d["light"]["on"]=spa.light;
    d["m7"]["status"]=spa.fault==FaultCode::NONE?"CHECK":"FAULT";d["m7"]["flow_ok"]=false;d["m7"]["note"]="No independently proven flow input";
    d["mode"]["heating"]=liveMode();d["mode"]["service"]=serviceMode;d["mode"]["demo"]=(bool)SPA_BENCH_DEMO;
    d["schedule"]["enabled"]=solarStart!=solarEnd;d["schedule"]["start"]=liveTime(solarStart);d["schedule"]["end"]=liveTime(solarEnd);auto days=d["schedule"]["days"].to<JsonArray>();for(int i=1;i<=7;++i)days.add(i);
    d["fault"]["active"]=spa.fault!=FaultCode::NONE;if(spa.fault!=FaultCode::NONE){const LiveEvent *record=nullptr;for(uint8_t n=0;n<liveEventCount;++n){const auto &r=liveEvents[(liveEventHead+7-n)%8];if(r.fault==spa.fault){record=&r;break;}}liveFaultJSON(d["fault"].as<JsonObject>(),spa.fault,record);}else{d["fault"]["id"]="";d["fault"]["panel_code"]="";d["fault"]["title"]="";}
    d["network"]["wifi_connected"]=WiFi.status()==WL_CONNECTED;d["network"]["rssi_dbm"]=WiFi.RSSI();d["network"]["matter_paired"]=false;liveSend(200,d);
}
void liveCapabilities(){JsonDocument d;d["api_version"]=1;d["heater_commissioned"]=false;d["target"]["min"]=20;d["target"]["max"]=40;d["target"]["step"]=0.5;auto modes=d["heating_modes"].to<JsonArray>();modes.add("OFF");modes.add("SOLAR_PRIORITY");d["schedule"]["precision_minutes"]=60;d["schedule"]["weekdays_editable"]=false;d["schedule"]["enabled_by_window"]=true;d["events_max"]=8;d["simulation_inputs"]=(bool)SPA_BENCH_DEMO;liveSend(200,d);}
void liveHistory(){JsonDocument d;auto list=d.to<JsonArray>();size_t size=2;for(uint8_t n=0;n<liveEventCount;++n){auto &r=liveEvents[(liveEventHead+7-n)%8];JsonDocument item;auto obj=item.to<JsonObject>();if(r.fault!=FaultCode::NONE)liveFaultJSON(obj,r.fault,&r);else{obj["id"]=r.id;obj["title"]=r.title;obj["severity"]="INFO";obj["timestamp"]=nullptr;obj["uptime_ms"]=r.at;}size_t next=measureJson(item)+1;if(size+next>3500)break;list.add(obj);size+=next;}liveSend(200,d);}
void liveDiagnostics(){JsonDocument d;d["raw_adc"]=nullptr;d["note"]="Raw ADC samples not retained by baseline; logical commands are not feedback.";d["heater_pins_inhibited"]=true;d["settings_healthy"]=storageHealthy;if(serviceMode){liveNumber(d["resistance_ohms"]["a"],tempA.resistance);liveNumber(d["resistance_ohms"]["b"],tempB.resistance);d["permissives"]["sensors_valid"]=liveInputsSafe();d["permissives"]["flow_ok"]=false;d["permissives"]["heater_allowed"]=false;}liveSend(200,d);}
bool liveFields(JsonObject obj,const char *fields,size_t count){if(obj.size()!=count)return false;for(auto kv:obj){std::string key=","+std::string(kv.key().c_str())+",";if(std::string(fields).find(key)==std::string::npos)return false;}return true;}
// ArduinoJson accepts duplicate keys/trailing text by design. Reject both
// before parsing commands; field names must be literal, not escaped aliases.
bool liveStrictObject(const std::string &raw){
    bool quoted=false,escape=false,ended=false;int depth=0;size_t quoteStart=0;std::set<std::string> keys;
    for(size_t i=0;i<raw.size();++i){char c=raw[i];
        if(ended){if(!isspace((unsigned char)c))return false;continue;}
        if(quoted){if(escape){escape=false;continue;}if(c=='\\'){escape=true;continue;}if(c=='"'){
            quoted=false;size_t next=i+1;while(next<raw.size()&&isspace((unsigned char)raw[next]))++next;
            if(depth==1&&next<raw.size()&&raw[next]==':'){std::string key=raw.substr(quoteStart,i-quoteStart);if(key.find('\\')!=std::string::npos||!keys.insert(key).second)return false;}
        }continue;}
        if(c=='"'){quoted=true;quoteStart=i+1;continue;}
        if(c=='{'||c=='['){if(depth==0&&c!='{')return false;++depth;}
        else if(c=='}'||c==']'){if(--depth<0)return false;if(depth==0)ended=true;}
        else if(depth==0&&!isspace((unsigned char)c))return false;
    }
    return ended&&!quoted&&!escape&&depth==0;
}

void liveCommand(const std::string &path){
    JsonDocument d;auto error=deserializeJson(d,web.jsonBody(),DeserializationOption::NestingLimit(3));
    if(error||!liveStrictObject(web.jsonBody())||!d.is<JsonObject>()){liveReply(400,"Malformed JSON object");return;}
    auto obj=d.as<JsonObject>();String command;bool stop=false;
    if(path=="target"){
        if(!liveFields(obj,",target_c,",1)||!obj["target_c"].is<double>()){liveReply(400,"Expected numeric target_c only");return;}
        double value=obj["target_c"].as<double>();if(!isfinite(value)||value<20||value>40||fabs(value*2-round(value*2))>1e-6){liveReply(422,"Target must be 20–40C in 0.5C UI steps");return;}command="setpoint "+String((float)value,1);
    }else if(path=="jets"){
        if(!liveFields(obj,",requested,",1)||!obj["requested"].is<const char*>()){liveReply(400,"Expected requested only");return;}
        std::string mode=obj["requested"].as<const char*>();if(mode!="OFF"&&mode!="LOW"&&mode!="HIGH"){liveReply(422,"Invalid requested speed");return;}command=mode=="OFF"?"off":mode=="LOW"?"low":"high";stop=mode=="OFF";
    }else if(path=="light"){
        if(!liveFields(obj,",on,",1)||!obj["on"].is<bool>()){liveReply(400,"Expected boolean on only");return;}bool desired=obj["on"].as<bool>();stop=!desired;if(desired!=spa.light)command="light";
    }else if(path=="mode"){
        if(!liveFields(obj,",heating,",1)||!obj["heating"].is<const char*>()){liveReply(400,"Expected heating only");return;}
        std::string mode=obj["heating"].as<const char*>();if(mode!="OFF"&&mode!="SOLAR_PRIORITY"){liveReply(422,"NORMAL/manual override is not supported by this adapter");return;}command=mode=="OFF"?"heatmode off":"heatmode solar";stop=mode=="OFF";
    }else if(path=="schedule"){
        if(!liveFields(obj,",enabled,,start,,end,,days,",4)||!obj["enabled"].is<bool>()||!obj["start"].is<const char*>()||!obj["end"].is<const char*>()||!obj["days"].is<JsonArray>()){liveReply(400,"Expected enabled,start,end,days");return;}
        auto parseHour=[](const char *time)->int{if(strlen(time)!=5||time[2]!=':'||time[3]!='0'||time[4]!='0'||time[0]<'0'||time[0]>'2'||time[1]<'0'||time[1]>'9')return -1;int h=(time[0]-'0')*10+time[1]-'0';return h<24?h:-1;};
        int start=parseHour(obj["start"]),end=parseHour(obj["end"]);auto days=obj["days"].as<JsonArray>();bool allDays=days.size()==7;uint8_t seen=0;for(auto day:days){if(!day.is<int>()||day.as<int>()<1||day.as<int>()>7){allDays=false;break;}seen|=1<<(day.as<int>()-1);}allDays=allDays&&seen==127;
        if(start<0||end<0||!allDays||obj["enabled"].as<bool>()!=(start!=end)){liveReply(422,"Only whole-hour daily windows are supported; equal endpoints disable");return;}command="window "+String(start)+"-"+String(end);
    }else if(path=="service-mode"){
        if(!liveFields(obj,",enabled,,confirm,",2)||!obj["enabled"].is<bool>()||!obj["confirm"].is<bool>()||!obj["confirm"].as<bool>()){liveReply(400,"Expected enabled and confirm:true");return;}bool enabled=obj["enabled"].as<bool>();command=enabled?"service on":"service off confirm";stop=enabled;
    }else if(path=="fault-reset"){
        if(!liveFields(obj,",confirm,",1)||!obj["confirm"].is<bool>()||!obj["confirm"].as<bool>()){liveReply(400,"Expected confirm:true");return;}command="resetfault";
    }else{liveReply(404,"Unknown high-level command");return;}
    bool exceptional=path=="service-mode"||path=="fault-reset";
    if(!stop&&!exceptional&&(serviceMode||spa.fault!=FaultCode::NONE||!storageHealthy)){liveReply(409,"Fault/Service/settings lockout");return;}
    if(!stop&&liveCommandSeen&&millis()-liveLastCommandMs<250){liveReply(429,"Command rate limited; refresh before retrying");return;}
    String result=command.length()?executeCommand(command):String("OK: already in requested state");
    if(!result.startsWith("OK")){liveReply(409,result.c_str());return;}
    if(!stop){liveCommandSeen=true;liveLastCommandMs=millis();}
    liveRecord("API_REQUEST",command.c_str());liveObserveFault();liveReply(200,result.c_str(),true);
}
void liveRoutes(){
    for(const auto &asset:spaAssets){const SpaAsset *p=&asset;web.on(asset.path,HTTP_GET,[p](){if(webAuth())web.sendAsset(p->type,p->data,p->size);});}
    web.on("/",HTTP_GET,[](){if(!webAuth())return;for(const auto &a:spaAssets)if(!strcmp(a.path,"/index.html")){web.sendAsset(a.type,a.data,a.size);return;}});
    web.on("/runtime.js",HTTP_GET,[](){if(webAuth())web.send(200,"text/javascript","export const runtime=Object.freeze({mode:'live',apiVersion:1});");});
    web.on("/api/state",HTTP_GET,[](){if(webAuth())liveState();});web.on("/api/capabilities",HTTP_GET,[](){if(webAuth())liveCapabilities();});
    web.on("/api/faults",HTTP_GET,[](){if(webAuth())liveHistory();});web.on("/api/diagnostics",HTTP_GET,[](){if(webAuth())liveDiagnostics();});
    for(const char *name:{"target","jets","light","mode","schedule","service-mode","fault-reset"}){std::string route=name;web.on(("/api/"+route).c_str(),HTTP_POST,[route](){if(webAuth())liveCommand(route);});}
}
void setup(){spa062Setup();liveRoutes();liveRecord("LIVE_API_STARTED","Live API adapter started; heater outputs inhibited");liveObserveFault();}
void loop(){spa062Loop();liveObserveFault();}
