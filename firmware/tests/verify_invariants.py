from pathlib import Path
import re,json,hashlib
root=Path(__file__).resolve().parent.parent
old=(root/'baseline/firmware_v061.txt').read_text();new=(root/'libraries/SpaController/src/SpaController.h').read_text()
pins=lambda s:dict(re.findall(r'(PIN_RELAY_\w+)\s*=\s*(\d+)',s))
assert pins(old)==pins(new)
start='bool schedulePermitsHeating() {'
def schedule(s):return s[s.index(start):s.index('String statusJSON()',s.index(start))]
assert schedule(old)==schedule(new)
constants=lambda s:dict(re.findall(r'static constexpr (?:uint32_t|float) ((?:DAILY_FILTER|AFTER_USE|READY_|SAMPLE_|HEAT_PUMP|JETS_|FILTER_CLEAN|STANDBY_)[A-Z_]+)\s*=\s*([^;]+);',s))
assert constants(old)==constants(new)
baseline=json.loads((root/'baseline/test-results-57.json').read_text());actual=json.loads((root/'logs/test-results.json').read_text())
assert {(r['build'],r['case']) for r in baseline}=={(r['build'],r['case']) for r in actual}
assert len(actual)==57 and all(r['passed'] for r in actual)
assert 'static constexpr bool HEATER_OUTPUTS_COMMISSIONED = false;' in new
assert 'pin == PIN_RELAY_HEAT_SAFE || pin == PIN_RELAY_HEAT_SSR) on = false;' in new
print('PASS: relay GPIOs, schedule predicate, scheduling constants, 57 baseline identities and heater locks preserved')
