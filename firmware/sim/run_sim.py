#!/usr/bin/env python3
"""Firmware-in-the-loop plant simulation of the REAL V0.9 controller (host only).

Builds sim/fw_sim.cpp (Main build, onboard ADC, SPA_SIM_COMMISSIONED which refuses
ESP32 builds), runs every scenario in parallel, writes sim/results.json and exits
non-zero if any scenario fails. Scenarios marked INFO document known limits.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json, os, subprocess, sys
root = Path(__file__).resolve().parent
exe = root / 'fw_sim'
subprocess.run(['g++', '-std=c++17', '-O2', '-w', '-DSPA_WIFI_SSID="sim"', '-DSPA_WEB_PASS="sim-password-only"',
                '-I' + str(root.parent / 'tests'), str(root / 'fw_sim.cpp'), '-o', str(exe)], check=True)
names = subprocess.run([str(exe), '--list'], capture_output=True, text=True, check=True).stdout.split()
def run(n):
    p = subprocess.run([str(exe), n], capture_output=True, text=True, timeout=900)
    try: return json.loads(p.stdout.strip().splitlines()[-1])
    except Exception: return {'scenario': n, 'verdict': 'FAIL: no output ' + p.stderr[-300:]}
with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
    rows = sorted(ex.map(run, names), key=lambda r: r['scenario'])
(root / 'results.json').write_text(json.dumps(rows, indent=1))
bad = [r for r in rows if not r['verdict'].startswith('PASS')]
for r in rows:
    if not r['scenario'].startswith('Z') or r in bad:
        print(f"{r['scenario']:<42} {r['verdict'][:140]}")
fz = [r for r in rows if r['scenario'].startswith('Z')]
print(f"fuzz: {len(fz) - sum(r in bad for r in fz)}/{len(fz)} pass")
print(f"TOTAL {len(rows)} scenarios, {len(rows) - len(bad)} pass")
sys.exit(1 if bad else 0)
