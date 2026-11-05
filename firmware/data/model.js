export const LIMITS = Object.freeze({ min: 20, max: 40, step: 0.5, staleMs: 6000 });
export const SCENARIOS = Object.freeze([
  ['idle', 'Normal idle'], ['heating', 'Heating normally'], ['reached', 'Setpoint reached'],
  ['high', 'Jets HIGH'], ['sensor-a', 'Sensor A fault'], ['sensor-b', 'Sensor B fault'],
  ['mismatch', 'Sensor mismatch'], ['flow', 'Low flow / excessive ΔT'],
  ['no-response', 'Heater no-response'], ['overtemp', 'Overtemperature'],
  ['wifi', 'Wi-Fi disconnected'], ['watchdog', 'Watchdog recovered']
]);
export const clone = value => structuredClone(value);
export function baseState() {
  return {
    controller: { online: true, uptime_s: 48231, firmware: 'demo-ui-0.1.0', build: 'web-milestone-1', reset_reason: 'power_on', watchdog_ok: true },
    temperature: { water_c: 36.8, target_c: 38, sensor_a_c: 36.7, sensor_b_c: 37.1, delta_t_c: 0.4, rise_rate_c_min: 0 },
    pump: { requested: 'OFF', actual: 'OFF' },
    heater: { demand: false, ssr_command: false, safety_contactor: false, allowed: true },
    uv: { on: false }, light: { on: false }, m7: { status: 'OK', flow_ok: true },
    mode: { heating: 'SOLAR_PRIORITY', service: false, demo: true },
    schedule: { enabled: true, start: '11:00', end: '16:00', days: [1,2,3,4,5,6,7] },
    fault: { active: false, panel_code: '', id: '', title: '' },
    network: { wifi_connected: true, rssi_dbm: -55, matter_paired: false }
  };
}
const definitions = {
  'sensor-a': ['Sn', 'SENSOR_A_RANGE', 'Sensor A reading invalid', 'Sensor A is outside its valid measurement range.', ['Sensor unplugged or failed', 'Damaged cable or connector corrosion', 'Input circuit fault'], ['Isolate power before inspecting connectors.', 'Check the Sensor A cable and connector.', 'Compare sensor resistance with the verified OEM curve.']],
  'sensor-b': ['Sn', 'SENSOR_B_RANGE', 'Sensor B reading invalid', 'Sensor B is outside its valid measurement range.', ['Sensor unplugged or failed', 'Damaged cable or connector corrosion', 'Input circuit fault'], ['Isolate power before inspecting connectors.', 'Check the Sensor B cable and connector.', 'Compare sensor resistance with the verified OEM curve.']],
  mismatch: ['Sn', 'SENSOR_MISMATCH', 'Heater sensors disagree', 'A/B disagreement exceeded the simulated threshold for 10 seconds.', ['Sensor calibration error', 'Poor probe contact', 'Unequal water temperature or restricted circulation'], ['Check both probe connections and placement.', 'Compare both probes at the same known temperature.']],
  flow: ['dr', 'DELTA_T_HIGH', 'Low flow suspected', 'Heater ΔT reached 4.0°C in this simulated low-flow scenario.', ['Blocked filter or restricted plumbing', 'Air lock or low water level', 'Weak circulation'], ['Check water level and circulation.', 'Inspect filter and any closed valves.', 'Do not reset until the cause has been checked.']],
  'no-response': ['—', 'HEAT_NO_RESPONSE', 'Heating has no thermal response', 'The simulated heating timeout elapsed without the expected temperature rise.', ['SSR failed open or contactor not closing', 'Failed heater element', 'Open switching or load path'], ['Inspect the event snapshot.', 'Have a qualified technician test the switching and heater circuit.']],
  overtemp: ['OH', 'OVER_TEMP', 'Water overtemperature', 'The simulated probe temperature reached 43.0°C.', ['Unintended heating', 'Poor circulation', 'Sensor or switching fault'], ['Keep heating isolated.', 'Allow water to cool and check with an independent thermometer.', 'Have the protective circuit inspected before reuse.']],
  watchdog: ['—', 'WATCHDOG_RECOVERY', 'Watchdog recovery lockout', 'The simulated controller restarted after a missed watchdog deadline.', ['Blocked task or firmware stall', 'Power instability', 'Peripheral communication failure'], ['Review the captured reset reason and event history.', 'Acknowledge the cleared fault, then explicitly exit Service Mode.']]
};
export function faultRecord(key, state, now = Date.now()) {
  const [panel_code,id,title,trigger_condition,probable_causes,recommended_checks] = definitions[key];
  return { timestamp: new Date(now).toISOString(), panel_code, id, title, severity: 'FAULT', trigger_condition,
    probable_causes, recommended_checks, retry_count: 0, resettable: key === 'watchdog', reset_at: null,
    snapshot: { sensor_a_c: state.temperature.sensor_a_c, sensor_b_c: state.temperature.sensor_b_c,
      delta_t_c: state.temperature.delta_t_c, setpoint_c: state.temperature.target_c,
      pump: state.pump.actual, heater: state.heater.ssr_command, uv: state.uv.on },
    threshold: key === 'flow' ? 'Simulated ΔT 4.0°C' : key === 'overtemp' ? 'Simulated trip at 42°C' : 'Demo scenario; not a commissioned threshold' };
}
export function scenarioState(key, now = Date.now()) {
  if (!SCENARIOS.some(([id]) => id === key)) throw new Error('Unknown demo scenario');
  const s = baseState();
  if (key === 'heating' || key === 'no-response') {
    s.pump = { requested: 'LOW', actual: 'LOW' }; s.uv.on = true;
    s.heater = { demand: true, ssr_command: true, safety_contactor: true, allowed: true };
    s.temperature.rise_rate_c_min = key === 'heating' ? 0.12 : 0;
  }
  if (key === 'reached') { Object.assign(s.temperature,{water_c:38,sensor_a_c:37.9,sensor_b_c:38.1,delta_t_c:0.2}); s.pump = {requested:'LOW',actual:'LOW'};s.uv.on=true; }
  if (key === 'high') { s.pump={requested:'HIGH',actual:'HIGH'};s.heater.allowed=false; }
  if (key === 'sensor-a' || key === 'sensor-b') {
    s.temperature[key === 'sensor-a' ? 'sensor_a_c' : 'sensor_b_c'] = null;
    s.temperature.water_c = null; s.temperature.delta_t_c = null;
  }
  if (key === 'mismatch' || key === 'flow') Object.assign(s.temperature,{sensor_a_c:36,sensor_b_c:40,delta_t_c:4,water_c:38});
  if (key === 'overtemp') Object.assign(s.temperature,{water_c:43,sensor_a_c:43,sensor_b_c:43,delta_t_c:0});
  if (key === 'wifi') { s.controller.online=false;s.network.wifi_connected=false;s.network.rssi_dbm=null;s.heater.allowed=false; }
  if (definitions[key]) {
    // Capture the trip context before applying the simulated protective state.
    const record=faultRecord(key,s,now);
    s.fault={active:true,...record};
    s.pump={requested:'OFF',actual:'OFF'};
    s.heater={demand:false,ssr_command:false,safety_contactor:false,allowed:false};
    s.uv.on=false;s.m7={status:'FAULT',flow_ok:false};
  }
  if (key === 'watchdog') { s.mode.service=true;s.controller.reset_reason='task_watchdog';s.controller.uptime_s=12; }
  return s;
}
export function isStale(lastUpdated, now = Date.now()) { return !lastUpdated || now-lastUpdated > LIMITS.staleMs; }
export function controlsBlocked(state, lastUpdated, now = Date.now()) {
  return !state || !state.controller.online || !state.network.wifi_connected || state.mode.service || state.fault.active || isStale(lastUpdated,now);
}
export function validateRequest(kind,payload) {
  const fields={target:['target_c'],jets:['requested'],light:['on'],mode:['heating'],schedule:['enabled','start','end','days'],'service-mode':['enabled','confirm'],'fault-reset':['confirm']};
  if(!payload||typeof payload!=='object'||Array.isArray(payload)||!fields[kind]||Object.keys(payload).some(key=>!fields[kind].includes(key)))throw new Error('Unsupported request fields.');
  if (kind==='target' && (Object.keys(payload).length!==1 || !Number.isFinite(payload.target_c) || payload.target_c<LIMITS.min || payload.target_c>LIMITS.max || Math.abs(payload.target_c/LIMITS.step-Math.round(payload.target_c/LIMITS.step))>1e-8)) throw new Error('Target must be 20–40°C in 0.5°C steps.');
  if (kind==='jets' && !['OFF','LOW','HIGH'].includes(payload.requested)) throw new Error('Invalid jets request.');
  if (kind==='light' && typeof payload.on!=='boolean') throw new Error('Light requires an on/off request.');
  if (kind==='mode' && !['NORMAL','SOLAR_PRIORITY','OFF'].includes(payload.heating)) throw new Error('Invalid heating mode.');
  if (kind==='service-mode' && (typeof payload.enabled!=='boolean' || payload.confirm!==true)) throw new Error('Service change requires explicit confirmation.');
  if (kind==='fault-reset' && payload.confirm!==true) throw new Error('Fault acknowledgement requires confirmation.');
  if (kind==='schedule') {
    const time=/^(?:[01]\d|2[0-3]):[0-5]\d$/;
    if (typeof payload.enabled!=='boolean' || !time.test(payload.start) || !time.test(payload.end) || !Array.isArray(payload.days) || !payload.days.length || new Set(payload.days).size!==payload.days.length || payload.days.some(d=>!Number.isInteger(d)||d<1||d>7)) throw new Error('Choose valid times and at least one unique weekday.');
  }
  if (!['target','jets','light','mode','schedule','service-mode','fault-reset'].includes(kind)) throw new Error('Unsupported high-level request.');
}
// Validate transport data before a partial/unsafe response replaces known state.
export function validateState(s) {
  const bool = value => typeof value==='boolean';
  if (!s || !s.controller || !s.temperature || !s.pump || !s.heater || !s.uv || !s.light || !s.m7 || !s.mode || !s.schedule || !s.fault || !s.network) throw new Error('Incomplete controller state.');
  for (const n of ['water_c','sensor_a_c','sensor_b_c','delta_t_c','rise_rate_c_min']) if (s.temperature[n]!==null && !Number.isFinite(s.temperature[n])) throw new Error('Invalid temperature state.');
  if (!Number.isFinite(s.temperature.target_c)||s.temperature.target_c<LIMITS.min||s.temperature.target_c>LIMITS.max || !['OFF','LOW','HIGH'].includes(s.pump.actual)||!['OFF','LOW','HIGH'].includes(s.pump.requested)) throw new Error('Invalid controller values.');
  if (![s.controller.online,s.controller.watchdog_ok,s.heater.demand,s.heater.ssr_command,s.heater.safety_contactor,s.heater.allowed,s.uv.on,s.light.on,s.mode.service,s.mode.demo,s.fault.active,s.network.wifi_connected].every(bool)) throw new Error('Invalid controller flags.');
  if(!['NORMAL','SOLAR_PRIORITY','OFF','MANUAL_LEGACY'].includes(s.mode.heating))throw new Error('Invalid mode state.');validateRequest('schedule',s.schedule);
  return s;
}

export function nextTarget(current,direction){const scaled=current/LIMITS.step;const value=(direction>0?Math.floor(scaled+1e-8)+1:Math.ceil(scaled-1e-8)-1)*LIMITS.step;return Math.min(LIMITS.max,Math.max(LIMITS.min,value));}
