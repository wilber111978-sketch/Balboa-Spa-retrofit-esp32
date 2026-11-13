#!/usr/bin/env python3
"""V0.9 behaviour scenarios: Demo build plus Main build with the onboard ADC."""
from pathlib import Path
import subprocess, json, re
root = Path(__file__).resolve().parent
src = (root / 'v09.cpp').read_text()
groups = {
    ('Demo', '1', '0'): 'uv-both-speeds uv-changeover uv-stopped manual-low-timeout manual-low-high-still-20 lockout-pump lockout-independent lockout-no-queue light-timeout light-program light-known-state light-restore-after-timeout light-bad-program priming priming-jets freeze freeze-one-sensor-high freeze-blind freeze-not-on-flow-fault poll-heat-off water-age no-reading-before-circulation floor-heats floor-not-above floor-window-setpoint flow-retry flow-latch water-hot heater-overtemp-latches sensor-mismatch sensor-cap ssr-proof status-fields'.split(),
    ('MainOnboard', '0', '0'): 'onboard-ntc onboard-38c onboard-open onboard-short onboard-no-i2c main-priming'.split(),
}
results = []
for (build, demo, ads), names in groups.items():
    exe = root / f'v09-{build}'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-DSPA_WIFI_SSID="test-network"',
                    '-DSPA_WEB_PASS="test-password-only"', f'-DSPA_BENCH_DEMO={demo}', f'-DSPA_TEMP_ADC_ADS1115={ads}',
                    '-I' + str(root), str(root / 'v09.cpp'), '-o', str(exe)], check=True)
    for case in names:
        assert re.search(r'test=="' + re.escape(case) + r'"', src), 'Unknown scenario: ' + case
        p = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=120)
        results.append(dict(build=build, case=case, passed=p.returncode == 0, output=p.stdout + p.stderr))
        print('PASS' if p.returncode == 0 else 'FAIL', build, case, p.stderr.strip())
(root.parent / 'logs/v09-results.json').write_text(json.dumps(results, indent=2))
raise SystemExit(any(not r['passed'] for r in results))
