#!/usr/bin/env python3
from pathlib import Path
import os,subprocess
from config import arduinojson_source
root=Path(__file__).resolve().parents[1];repo=root.parent
args=['g++','-std=c++17','-Wall','-Wextra','-Werror','-DSPA_BENCH_DEMO=1','-DSPA_WIFI_SSID="test-network"','-DSPA_WEB_PASS="test-password-only"','-include','SpaLiveApi.h']
for p in [root/'live-tests',root/'tests',root/'libraries/SpaLiveApi/src',root/'libraries/SpaController/src',arduinojson_source(root)]:args+=['-I'+str(p)]
subprocess.run(args+[str(root/'live-tests/bridge.cpp'),'-o',str(root/'live-tests/bridge')],check=True)
