#!/usr/bin/env python3
from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parent
names='service alloff off-semantics service-fault-exit service-save service-reboot watchdog-reboot watchdog-panic brownout watchdog-feed watchdog-feed-failure watchdog-init-failure watchdog-existing settings-open-failure settings-write-failure settings-short-write settings-readback settings-crc settings-version settings-missing settings-truncated settings-semantic settings-migration settings-roundtrip safety-crc runtime-crc fault-reset-save-failure service-exit-save-failure stale-before-refresh stale-b stale-wrap network-slow-header network-slow-body network-budget network-large-header network-stalled-response network-auth-status network-duplicate-length network-service network-tx-budget crc-known-vector settings-legacy-values'.split()
results=[]
for mode in (1,0):
 exe=root/f'extended-{mode}'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"',f'-DSPA_BENCH_DEMO={mode}','-I'+str(root),str(root/'extended.cpp'),'-o',str(exe)],check=True)
 for case in names:
  p=subprocess.run([str(exe),case,str(root.parent/f'logs/service-nvs-{mode}.txt')],capture_output=True,text=True,timeout=20)
  results.append(dict(build='Demo' if mode else 'Main',case=case,passed=p.returncode==0,output=p.stdout+p.stderr))
  print('PASS' if p.returncode==0 else 'FAIL',mode,case,p.stderr.strip())
(root.parent/'logs/extended-results.json').write_text(json.dumps(results,indent=2))
raise SystemExit(any(not r['passed'] for r in results))
