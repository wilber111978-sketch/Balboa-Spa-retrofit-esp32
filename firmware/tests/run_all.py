#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,json
root=Path(__file__).resolve().parent
for script in ['run_tests.py','run_extended.py','run_v09.py','verify_invariants.py']:
 subprocess.run([sys.executable,str(root/script)],check=True)
results=[]
for filename in ['test-results.json','extended-results.json','v09-results.json']:
 results.extend(json.loads((root.parent/'logs'/filename).read_text()))
(root.parent/'logs/all-results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f"{len(results)} executed scenarios; {sum(r['passed'] for r in results)} passed")
