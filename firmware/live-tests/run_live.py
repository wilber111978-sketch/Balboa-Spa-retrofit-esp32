#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,os
root=Path(__file__).resolve().parents[1];repo=root.parent
from config import arduinojson_source
json_src=arduinojson_source(root)
base=['g++','-std=c++17','-Wall','-Wextra','-Werror','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"','-DSPA_TEMP_ADC_ADS1115=1']
for p in [root/'live-tests',root/'tests',root/'libraries/SpaLiveApi/src',root/'libraries/SpaTopside/src',root/'libraries/SpaController/src',json_src]:base+=['-I'+str(p)]
results=[];cases='state capabilities auth target bounds strict-json jets light-idempotent service fault save-failure rate-limit schedule unsupported static-stream origin history diagnostics v09-state v09-light-program v09-lockout'.split()
for mode in (0,1):
 exe=root/'live-tests'/f'api-{mode}'
 subprocess.run(base+[f'-DSPA_BENCH_DEMO={mode}','-include',os.environ.get('SPA_TEST_WRAPPER','SpaLiveApi.h'),str(root/'live-tests/api.cpp'),'-o',str(exe)],check=True)
 for case in cases:
  p=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=20);print('PASS' if p.returncode==0 else 'FAIL',mode,case,p.stderr.strip());results.append({'build':'Demo' if mode else 'Main','case':case,'passed':p.returncode==0,'output':p.stdout+p.stderr})
# Optional Google Home / Matter bridge (SPA_MATTER 1), Main build, host stand-in for the Matter library.
exe=root/'live-tests'/'matter-0'
mbase=[x for x in base if x!='-DSPA_TEMP_ADC_ADS1115=1']
subprocess.run(mbase+['-DSPA_BENCH_DEMO=0','-DSPA_MATTER=1','-DSPA_MATTER_HOST_TEST','-I'+str(root/'live-tests/matter-stub'),'-include','SpaLiveApi.h',str(root/'live-tests/matter.cpp'),'-o',str(exe)],check=True)
for case in 'begin temperature light wake ready-timeout service-lockout fault-lockout light-from-dashboard pairing slow-begin'.split():
  p=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=20);print('PASS' if p.returncode==0 else 'FAIL','matter',case,p.stderr.strip());results.append({'build':'Main+Matter','case':case,'passed':p.returncode==0,'output':p.stdout+p.stderr})
(root/'live-logs/api-results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(not r['passed'] for r in results))
