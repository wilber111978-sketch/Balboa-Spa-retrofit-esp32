import {clone,scenarioState,validateRequest,validateState} from './model.js';

export class DemoSpaService {
  constructor({storage=globalThis.localStorage,now=()=>Date.now(),latency=100}={}) {
    this.storage=storage;this.now=now;this.latency=latency;this.history=[];this.scenario='heating';this.state=scenarioState(this.scenario,now());
    try {if(storage?.getItem('fisher.demo.service')==='true'){this.state.mode.service=true;this.stop();}}catch{/* Demo remains usable when browser storage is unavailable. */}
    this.event('DEMO_STARTED','Demo started. No hardware connected.');
  }
  async wait(){if(this.latency)await new Promise(resolve=>setTimeout(resolve,this.latency));}
  stop(){this.state.pump={requested:'OFF',actual:'OFF'};this.state.heater={demand:false,ssr_command:false,safety_contactor:false,allowed:false};this.state.uv.on=false;this.state.light.on=false;}
  event(id,title,record={}){this.history.unshift({timestamp:new Date(this.now()).toISOString(),id,title,severity:'INFO',...clone(record)});this.history=this.history.slice(0,50);}
  async read(){await this.wait();return {state:clone(this.state),events:clone(this.history),diagnostics:{raw_adc:null,resistance_ohms:null,permissives:{sensors_valid:this.state.temperature.sensor_a_c!==null&&this.state.temperature.sensor_b_c!==null,flow_ok:this.state.m7.flow_ok,heater_allowed:this.state.heater.allowed},note:'Raw ADC and resistance are unavailable in this simulator.'}};}
  async selectScenario(key){await this.wait();this.scenario=key;this.state=scenarioState(key,this.now());
    // Persistent Service Mode survives demo scene changes until explicitly exited.
    let retained=false;try{retained=this.storage?.getItem('fisher.demo.service')==='true';}catch{}
    if(retained||key==='watchdog'){this.state.mode.service=true;this.stop();this.persistService(true);}
    this.event('SCENARIO_CHANGED',`Scenario: ${key}`);
    if(this.state.fault.active)this.event(this.state.fault.id,this.state.fault.title,this.state.fault);
    return this.read();
  }
  persistService(on){try{this.storage?.setItem('fisher.demo.service',String(on));}catch{throw new Error('Browser storage could not save Service Mode. Simulator stopped; reload persistence is unavailable.');}}
  async request(kind,payload){validateRequest(kind,payload);await this.wait();const s=this.state;
    if(!s.controller.online||!s.network.wifi_connected)throw new Error('Controller offline. Request was not applied.');
    if(kind==='service-mode'){
      if(!payload.enabled&&s.fault.active)throw new Error('Acknowledge the cleared fault before exiting Service Mode.');
      // Stop first. A failed persistence attempt keeps the simulator inhibited.
      s.mode.service=true;this.stop();this.persistService(payload.enabled);s.mode.service=payload.enabled;
      if(!payload.enabled)s.heater.allowed=true;
      this.event('SERVICE_MODE',payload.enabled?'Service Mode enabled; all simulated outputs OFF.':'Service Mode exited.');
    }else if(kind==='fault-reset'){
      if(s.fault.active&&!s.fault.resettable)throw new Error('Unsafe inputs remain. Resolve the scenario cause before acknowledgement.');
      this.event('FAULT_RESET','Cleared fault acknowledged.',{snapshot:clone(s.fault.snapshot||{}),reset_at:new Date(this.now()).toISOString()});
      s.fault={active:false,panel_code:'',id:'',title:''};s.m7={status:'OK',flow_ok:true};s.heater.allowed=!s.mode.service;
    }else{
      if(s.mode.service||s.fault.active)throw new Error('Request rejected by simulated fault/Service lockout.');
      if(kind==='target'){s.temperature.target_c=payload.target_c;if(s.temperature.water_c>=payload.target_c){s.heater.demand=false;s.heater.ssr_command=false;s.heater.safety_contactor=false;}}
      if(kind==='jets'){
        s.pump={requested:payload.requested,actual:payload.requested};
        // HIGH and OFF always remove simulated heater permission first.
        s.heater.ssr_command=false;s.heater.safety_contactor=false;s.heater.demand=false;
        s.heater.allowed=payload.requested==='LOW';s.uv.on=payload.requested==='LOW';
      }
      if(kind==='light')s.light.on=payload.on;
      if(kind==='mode')s.mode.heating=payload.heating;
      if(kind==='schedule')s.schedule=clone(payload);
      this.event('REQUEST_ACCEPTED',`${kind} request accepted by demo controller.`);
    }
    try{return await this.read();}catch{throw new Error('Controller accepted the request, but state refresh failed. Refresh before retrying.');}
  }
}

// Future transport for docs/API_CONTRACT.md, NOT an adapter for V0.6.2 /api/cmd.
// The UI does not instantiate this class in milestone 1.
export class HttpSpaService {
  constructor({base='',fetchFn=globalThis.fetch.bind(globalThis),timeoutMs=2500,requireCapabilities=false}={}){this.base=base;this.fetchFn=fetchFn;this.timeoutMs=timeoutMs;this.requireCapabilities=requireCapabilities;this.capabilities=null;}
  async json(path,options={}){
    const abort=new AbortController();const timer=setTimeout(()=>abort.abort(),this.timeoutMs);
    try{
      const response=await this.fetchFn(this.base+path,{...options,signal:abort.signal,credentials:'same-origin',cache:'no-store'});
      if(!response.ok)throw new Error(`Controller rejected request (HTTP ${response.status}).`);
      const text=await response.text();if(text.length>65536)throw new Error('Controller response too large.');
      return JSON.parse(text);
    }catch(error){if(error.name==='AbortError')throw new Error('Controller timed out. Request outcome is unknown; refresh before retrying.');throw error;}
    finally{clearTimeout(timer);}
  }
  async read(){if(this.requireCapabilities&&!this.capabilities){const caps=await this.json('/api/capabilities');if(caps.api_version!==1||caps.heater_commissioned!==false||!Array.isArray(caps.heating_modes)||caps.schedule?.precision_minutes!==60)throw new Error('Unsupported controller API capabilities.');this.capabilities=caps;}const [state,events,diagnostics]=await Promise.all([this.json('/api/state'),this.json('/api/faults'),this.json('/api/diagnostics')]);validateState(state);if(!Array.isArray(events)||events.length>50)throw new Error('Invalid bounded event list.');return {state,events,diagnostics,capabilities:this.capabilities};}
  async request(kind,payload){validateRequest(kind,payload);const ack=await this.json('/api/'+kind,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    if(ack?.accepted!==true)throw new Error(ack?.message||'Controller did not acknowledge this request.');
    // Never turn the submitted values into state. Read the controller after ACK.
    try{return await this.read();}catch{throw new Error('Controller accepted the request, but state refresh failed. Refresh before retrying.');}
  }
}
