#!/usr/bin/env python3
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'FROZEN_V062.json').read_text())
for path,expected in manifest['files'].items():
 actual=hashlib.sha256((root/path).read_bytes()).hexdigest()
 if actual!=expected:raise SystemExit('Frozen baseline modified: '+path)
print(f"PASS: {len(manifest['files'])} V0.6.2 files match pinned commit {manifest['firmware_commit'][:7]}")
