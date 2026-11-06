#!/usr/bin/env python3
"""Run unchanged baseline scenario sources with the live wrapper preincluded."""
from pathlib import Path
import ast,subprocess,json,os
root=Path(__file__).resolve().parents[1];repo=root.parent
from config import arduinojson_source
json_src=arduinojson_source(root)
base=['g++','-std=c++17','-Wall','-Wextra','-Werror','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"']
for p in [root/'live-tests',root/'tests',root/'libraries/SpaLiveApi/src',root/'libraries/SpaTopside/src',root/'libraries/SpaController/src',json_src]:base+=['-I'+str(p)]
base+=['-include',os.environ.get('SPA_TEST_WRAPPER','SpaLiveApi.h')];results=[]
old=ast.parse((root/'tests/run_tests.py').read_text());node=next(n.value for n in old.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets));cases={k.value:v.func.value.value.split() for k,v in zip(node.keys,node.values)}
ext=ast.parse((root/'tests/run_extended.py').read_text());names=next(n.value.func.value.value.split() for n in ext.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='names' for t in n.targets))
for source,groups in [('scenarios.cpp',cases),('extended.cpp',{1:names,0:names})]:
 for mode,items in groups.items():
  exe=root/'live-tests'/f'regression-{source}-{mode}'
  subprocess.run(base+[f'-DSPA_BENCH_DEMO={mode}',str(root/'tests'/source),'-o',str(exe)],check=True)
  for case in items:
   fixture='service' if source=='extended.cpp' else 'power'
   p=subprocess.run([str(exe),case,str(root/'live-logs'/f'{fixture}-nvs-{mode}.txt')],capture_output=True,text=True,timeout=20)
   print('PASS' if p.returncode==0 else 'FAIL',mode,case,p.stderr.strip());results.append({'build':'Demo' if mode else 'Main','case':case,'passed':p.returncode==0,'output':p.stdout+p.stderr})
(root/'live-logs/regression-results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(not r['passed'] for r in results))
