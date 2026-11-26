#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'topside-logs';out.mkdir(exist_ok=True)
base=['g++','-std=c++17','-Wall','-Wextra','-Werror','-pthread','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"','-DSPA_TEMP_ADC_ADS1115=1']
for p in [root/'live-tests',root/'tests',root/'libraries/SpaLiveApi/src',root/'libraries/SpaController/src',root/'libraries/SpaTopside/src',Path(os.environ['ARDUINOJSON_SRC'])]:base+=['-I'+str(p)]
cases='startup debounce light-long repeat limits preview jets-heating service fault-priority invalid-temperature stuck boot-held wifi-loss rapid-conflicts simultaneous display-disconnect watchdog-restart brownout-restart render-delay persist-failure schedule-unchanged v09-priming-end v09-light-colour v09-reading-age v09-no-reading'.split()
results=[]
for mode in (0,1):
 exe=out/f'topside-{mode}'
 subprocess.run(base+[f'-DSPA_BENCH_DEMO={mode}',str(root/'topside-tests/scenarios.cpp'),'-o',str(exe)],check=True)
 for name in cases:
  p=subprocess.run([str(exe),name],capture_output=True,text=True,timeout=10)
  print(mode,name,'PASS' if p.returncode==0 else 'FAIL',p.stderr.strip())
  results.append(dict(build='Demo' if mode else 'Main',case=name,passed=p.returncode==0,output=p.stdout+p.stderr))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(not r['passed'] for r in results))
