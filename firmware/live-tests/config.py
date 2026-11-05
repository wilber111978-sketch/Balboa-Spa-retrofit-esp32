from pathlib import Path
import os

def arduinojson_source(root):
    override=os.environ.get('ARDUINOJSON_SRC')
    candidates=[Path(override)] if override else [Path.home()/'Arduino/libraries/ArduinoJson/src',root.parent.parent/'toolchain/sketchbook/libraries/ArduinoJson/src']
    for path in candidates:
        if (path/'ArduinoJson.h').is_file():return path
    raise SystemExit('Install ArduinoJson 7.4.2 and set ARDUINOJSON_SRC to its src directory.')
