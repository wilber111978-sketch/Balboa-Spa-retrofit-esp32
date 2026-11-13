"""V0.9 invariants.

V0.9 is the first release that edits the V0.6.2 controller. These checks pin
what must NOT change, and confirm the deliberate V0.9 changes are present.
"""
from pathlib import Path
import re, json
root = Path(__file__).resolve().parent.parent
old = (root / 'baseline/firmware_v061.txt').read_text()
new = (root / 'libraries/SpaController/src/SpaController.h').read_text()

# 1. Relay GPIOs identical to the reviewed baseline (CH1 safety, CH2 LOW, CH3 SSR, CH4 HIGH, CH5 UV, CH6 light).
pins = lambda s: dict(re.findall(r'(PIN_RELAY_\w+)\s*=\s*(\d+)', s))
assert pins(old) == pins(new), 'relay GPIO map changed'

# 2. Filtration / scheduling constants unchanged.
constants = lambda s: dict(re.findall(r'static constexpr (?:uint32_t|float) ((?:DAILY_FILTER|AFTER_USE|READY_|SAMPLE_|HEAT_PUMP|JETS_|FILTER_CLEAN|STANDBY_)[A-Z_]+)\s*=\s*([^;]+);', s))
assert constants(old) == constants(new), 'baseline scheduling constants changed'

# 3. Schedule predicate: V0.9 only adds the economy floor; heat mode OFF still never heats.
start = 'bool schedulePermitsHeating() {'
pred = new[new.index(start):new.index('String statusJSON()', new.index(start))]
assert 'if (heatMode == 0) return false;' in pred and 'if (heatMode == 2) return true;' in pred
assert 'floorHeatNeeded' in pred and 'solarWindowOpen(minute)' in pred

# 4. Physical heater outputs still locked pending commissioning.
assert 'static constexpr bool HEATER_OUTPUTS_COMMISSIONED = false;' in new
assert 'pin == PIN_RELAY_HEAT_SAFE || pin == PIN_RELAY_HEAT_SSR) on = false;' in new

# 5. Every one of the 57 reviewed baseline scenarios is still present and passing.
baseline = json.loads((root / 'baseline/test-results-57.json').read_text())
actual = json.loads((root / 'logs/test-results.json').read_text())
have = {(r['build'], r['case']): r['passed'] for r in actual}
missing = [(r['build'], r['case']) for r in baseline if (r['build'], r['case']) not in have]
assert not missing, f'baseline scenarios missing: {missing}'
assert all(have[(r['build'], r['case'])] for r in baseline), 'a baseline scenario failed'
assert all(r['passed'] for r in actual)

# 6. V0.9 behaviour scenarios all pass.
v09 = json.loads((root / 'logs/v09-results.json').read_text())
assert len(v09) >= 39 and all(r['passed'] for r in v09), 'V0.9 scenario failure'

print(f'PASS: relay GPIOs, scheduling constants, heater locks preserved; '
      f'{len(baseline)} baseline + {len(v09)} V0.9 scenarios pass')
