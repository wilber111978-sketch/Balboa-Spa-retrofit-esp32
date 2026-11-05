#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,re
root=Path(__file__).resolve().parent
cases={1:'startup heat hysteresis changeover fault-high sensor-b overtemp disagreement diff-inhibit highlimit invalid-range filtration schedule-heat overnight equal-window catchup afteruse afteruse-pause jets-timeout ready-timeout runtime day-rollover nonfinite filter-threshold clock-backwards stale json rapid-commands rollover wifi-loss malformed power-save power-recover web-auth web-input heat-runon fault-v-schedule rapid-rise runon-wrap clock-loss clean-confirm corrupt-config restart-fault restart-manual restart-afteruse'.split(),0:'main-injections main-lock adc-startup adc-disconnect adc-timeout ntc main-ema raw-overtemp corrupt-config restart-fault restart-manual restart-afteruse'.split()}
results=[]
for mode, names in cases.items():
 exe=root/f'test-{mode}'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"',f'-DSPA_BENCH_DEMO={mode}','-I'+str(root),str(root/'scenarios.cpp'),'-o',str(exe)],check=True)
 for case in names:
  assert re.search(r'test=="'+re.escape(case)+r'"', (root/"scenarios.cpp").read_text()), "Unknown scenario: "+case
  p=subprocess.run([str(exe),case,str(root.parent/"logs/power-nvs.txt")],capture_output=True,text=True)
  results.append(dict(build='Demo' if mode else 'Main',case=case,passed=p.returncode==0,output=p.stdout+p.stderr))
  print(('PASS' if p.returncode==0 else 'FAIL'),mode,case,p.stderr.strip())
  if case=='json' and p.returncode==0:json.loads(p.stdout.splitlines()[0])
(root.parent/'logs/test-results.json').write_text(json.dumps(results,indent=2))
raise SystemExit(any(not r['passed'] for r in results))
