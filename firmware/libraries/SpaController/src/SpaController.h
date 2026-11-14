#pragma once
/*
  BALBOA SPA RETROFIT - ESP32-S3
  V0.9 (V0.6.2 controller + V0.9 control additions; see docs/V0_9_CHANGES.md)

  Controller:
    Waveshare ESP32-S3 Relay 6CH

  Temperature sensors:
    2 x Balboa M7 / 53605
    Model for initial firmware:
      R25  = 30,000 ohms
      Beta = 3860 K
    Calibration can be changed later.

  IMPORTANT ARCHITECTURE:
    - Remote commands NEVER write relays directly.
    - Heater permission is calculated centrally.
    - Pump LOW/HIGH cannot run simultaneously.
    - Heater requires circulation.
    - Dual temperature sensors must remain plausible.
    - Critical faults immediately remove heater permission.
    - Mechanical / independent high-limit protection is still required.
*/

#include <Arduino.h>
#ifndef SPA_BENCH_DEMO
#define SPA_BENCH_DEMO 0
#endif
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>
#include <WiFi.h>
#include "BoundedWebServer.h"
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <cstddef>
#include <time.h>
#include <algorithm>

// ============================================================
// PIN MAP - WAVESHARE ESP32-S3 RELAY 6CH
// ============================================================

// Relay-channel order confirmed by the existing sketch; verify on the physical board.
static constexpr uint8_t PIN_RELAY_HEAT_SAFE = 1;   // CH1 - safety contactor
static constexpr uint8_t PIN_RELAY_PUMP_LOW  = 2;   // CH2 - circulation
static constexpr uint8_t PIN_RELAY_HEAT_SSR  = 41;  // CH3 - SSR control supply (via dry relay contacts)
static constexpr uint8_t PIN_RELAY_PUMP_HIGH = 42;  // CH4 - jets
static constexpr uint8_t PIN_RELAY_UV        = 45;  // CH5 - ozone/UV
static constexpr uint8_t PIN_RELAY_LIGHT     = 46;  // CH6 - light

static constexpr uint8_t PIN_RS485_TX = 17;
static constexpr uint8_t PIN_RS485_RX = 18;
static constexpr uint8_t PIN_BUZZER   = 21;
static constexpr uint8_t PIN_STATUS_RGB = 38;

// CH3 is a relay output, not an ESP32 GPIO direct SSR connection.
// Use correctly rated DC control power through CH3's dry contacts.
// SSR power path and heater safety contactor power contacts are in series.

// ============================================================
// OPTIONAL / FUTURE INPUTS
// ============================================================

// Assign once interface board is finalised.
// MAIN: must be physically implemented before live heater enable (proposed GPIO16).
#ifndef SPA_PIN_HIGH_LIMIT_OK
#define SPA_PIN_HIGH_LIMIT_OK -1
#endif
static constexpr int PIN_HIGH_LIMIT_OK       = SPA_PIN_HIGH_LIMIT_OK;
static constexpr int PIN_FB_PUMP_LOW         = -1;
static constexpr int PIN_FB_PUMP_HIGH        = -1;
static constexpr int PIN_FB_HEAT_CONTACTOR   = -1;
static constexpr int PIN_HEATER_CURRENT      = -1;

// ============================================================
// RELAY LOGIC
// ============================================================
//
// Confirm physical relay active polarity before connecting loads.
// Waveshare boards commonly use HIGH = relay ON, but bench verify.
//
// Change these two definitions only if necessary.
//
static constexpr uint8_t RELAY_ON  = HIGH;
static constexpr uint8_t RELAY_OFF = LOW;

// ============================================================
// TEMPERATURE MODEL
// ============================================================

static constexpr float DEFAULT_R25   = 30000.0f;
static constexpr float DEFAULT_BETA  = 3860.0f;
static constexpr float T0_KELVIN     = 298.15f; // 25 C

// Precision divider resistor
static constexpr float R_FIXED = 30000.0f;

// V0.9: temperature ADC backend.
//   0 = ESP32-S3 onboard ADC1 (default until an ADS1115 is fitted): sensors on GPIO8/GPIO9.
//   1 = ADS1115 on I2C (SDA GPIO8, SCL GPIO9): sensors on A0/A1.
#ifndef SPA_TEMP_ADC_ADS1115
#define SPA_TEMP_ADC_ADS1115 0
#endif
static constexpr uint8_t PIN_TEMP_A_ADC = 8;    // onboard mode: ADC1_CH7
static constexpr uint8_t PIN_TEMP_B_ADC = 9;    // onboard mode: ADC1_CH8
static constexpr uint8_t ONBOARD_ADC_SAMPLES = 32;
// Optional 3.3 V divider-rail reference for ratiometric conversion.
//   onboard mode: GPIO3 (ADC1_CH2) through a 10k/10k divider; ADS1115 mode: A2 direct.
static constexpr int  PIN_RAIL_REF_ADC = 3;
static constexpr bool RAIL_REF_FITTED = false;   // set true only after the reference is wired
// Optional 24 V supply monitor (ADS1115 mode only): A3 through 100k/10k (x11).
static constexpr bool SUPPLY_MON_FITTED = false;
static constexpr float SUPPLY_MON_RATIO = 11.0f;
static constexpr float SUPPLY_SAG_V = 20.0f, SUPPLY_OK_V = 21.0f;

// ADS1115 supply / divider supply
static constexpr float SENSOR_VCC = 3.300f;

// ============================================================
// TEMPERATURE SAFETY LIMITS
// ============================================================

// User-adjustable setpoint ceiling
static constexpr float USER_MAX_SETPOINT_C = 40.0f;

// Absolute software safety ceiling.
// Calibration can later be refined, but this ceiling remains.
static constexpr float ABSOLUTE_TEMP_TRIP_C = 42.0f;

// Very high / clearly abnormal measurement.
static constexpr float HARD_TEMP_LOCKOUT_C = 43.0f;

// Plausibility range
static constexpr float SENSOR_MIN_TEMP_C = 0.0f;
static constexpr float SENSOR_MAX_TEMP_C = 60.0f;

// Raw divider fault thresholds
static constexpr float SENSOR_SHORT_V = 0.15f;
static constexpr float SENSOR_OPEN_V  = 3.10f;
// Onboard ADC saturates near 3.1 V at 11 dB attenuation, so treat >= 3.00 V as open.
static constexpr float SENSOR_OPEN_V_ONBOARD = 3.00f;

// Dual sensor agreement
static constexpr float TEMP_DIFF_WARNING_C = 0.75f;
static constexpr float TEMP_DIFF_INHIBIT_C = 1.50f;
static constexpr float TEMP_DIFF_FAULT_C   = 2.50f;

// How long disagreement must exist before some faults trip.
static constexpr uint32_t TEMP_DIFF_FAULT_DELAY_MS = 10000;

// ============================================================
// FLOW / RATE OF RISE PROTECTION
// ============================================================

// These are deliberately conservative starting values.
// We will tune them during wet commissioning.

static constexpr float MAX_SINGLE_SENSOR_RISE_C_PER_SEC = 0.35f;
static constexpr float MAX_SENSOR_RATE_DIFFERENCE_C_SEC = 0.25f;

static constexpr uint32_t RATE_MONITOR_DELAY_MS = 5000;

// ============================================================
// PUMP TIMING
// ============================================================

static constexpr uint32_t PUMP_CHANGEOVER_DEADTIME_MS = 750;

static constexpr uint32_t MIN_CIRCULATION_BEFORE_HEAT_MS = 7000;

static constexpr uint32_t PUMP_RUNON_AFTER_HEAT_MS = 30000;

// ============================================================
// HEATER CONTROL
// ============================================================

// V0.3: ON at setpoint - 1.0 C; OFF at setpoint. No proportional relay cycling.
static constexpr float HEATER_HYSTERESIS_C = 1.0f; // Dynamic standby/ready threshold below
static constexpr uint32_t HEATER_CONTACTOR_SETTLE_MS = 1000;
// Development lock: M7-inspired qualification and independent safety input
// are not yet verified. Keep physical heater outputs DISABLED.
#ifdef SPA_SIM_COMMISSIONED
// Host plant simulation ONLY: lets the simulator exercise commissioned heater logic.
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO)
#error "SPA_SIM_COMMISSIONED is for host simulation only and must never be built for the ESP32"
#endif
static constexpr bool HEATER_OUTPUTS_COMMISSIONED = true;
#else
static constexpr bool HEATER_OUTPUTS_COMMISSIONED = false; // Live heater requires separate formal commissioning
#endif
static constexpr uint32_t M7_STABILISATION_MS = SPA_BENCH_DEMO ? 5000 : 120000;
// Do NOT add an M7 heat pulse until validated for the OEM hardware.


// ============================================================
// TASK TIMINGS
// ============================================================

static constexpr uint32_t TEMP_SAMPLE_MS  = 500;
static constexpr uint32_t CONTROL_LOOP_MS = 250;
static constexpr uint32_t STATUS_LOOP_MS  = 1000;

// ============================================================
// ADS1115
// ============================================================

Adafruit_ADS1115 ads;

// ============================================================
// NVS
// ============================================================

Preferences prefs;

// ============================================================
// SYSTEM ENUMS
// ============================================================

enum class SpaState : uint8_t
{
    BOOT,
    SELF_TEST,
    IDLE,
    CIRCULATING,
    HEATING,
    PUMP_HIGH,
    FILTER,
    FREEZE_PROTECT,
    SERVICE,
    FAULT,
    LOCKOUT
};

enum class PumpMode : uint8_t
{
    OFF,
    LOW_SPEED,
    HIGH_SPEED
};

enum class FaultCode : uint16_t
{
    NONE = 0,

    TEMP_A_OPEN            = 1,
    TEMP_A_SHORT           = 2,
    TEMP_B_OPEN            = 3,
    TEMP_B_SHORT           = 4,

    TEMP_A_INVALID         = 5,
    TEMP_B_INVALID         = 6,

    TEMP_DISAGREE          = 7,
    OVERTEMP               = 8,
    RAPID_TEMP_RISE        = 9,
    FLOW_SUSPECT           = 10,

    PUMP_INTERLOCK         = 11,
    LOW_CONTACTOR_FAULT    = 12,
    HIGH_CONTACTOR_FAULT   = 13,

    HEATER_CONTACTOR_FAULT = 14,
    HEATER_SSR_SUSPECT     = 15,

    HIGH_LIMIT_OPEN        = 16,

    ADC_FAILURE            = 17,
    CONFIG_CORRUPT         = 18,
    WATCHDOG_RESET         = 19,

    TOPSIDE_COMMS          = 20,
    POWER_FAULT            = 21,
    SENSOR_STALE           = 22
};

enum class SensorStatus : uint8_t
{
    OK,
    OPEN,
    SHORT,
    INVALID
};

// ============================================================
// TEMPERATURE STRUCT
// ============================================================

struct TemperatureChannel
{
    float voltage = 0.0f;
    float resistance = 0.0f;

    float rawC = NAN;
    float filteredC = NAN;

    float previousC = NAN;
    float rateCPerSec = 0.0f;

    SensorStatus status = SensorStatus::INVALID;

    uint32_t lastUpdateMs = 0;
};

TemperatureChannel tempA;
TemperatureChannel tempB;

// ============================================================
// CONFIGURATION
// ============================================================

struct SpaConfig
{
    float setpointC = 38.0f;

    float sensorAR25 = DEFAULT_R25;
    float sensorABeta = DEFAULT_BETA;
    float sensorAOffset = 0.0f;

    float sensorBR25 = DEFAULT_R25;
    float sensorBBeta = DEFAULT_BETA;
    float sensorBOffset = 0.0f;
};

SpaConfig config;
bool configurationValid = false;
bool storageHealthy = true;
bool serviceMode = false;
bool sensorsAcquired = false;
static constexpr uint32_t SENSOR_STALE_MS = 2000;
static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 5000;
esp_task_wdt_user_handle_t controlWatchdog = nullptr;
int resetReason = 0;
uint8_t heatMode = 0; // 0=off, 1=time window, 2=manual
uint8_t solarStart = 11, solarEnd = 16;


// ============================================================
// RUNTIME STATUS
// ============================================================

struct SpaRuntime
{
    SpaState state = SpaState::BOOT;

    PumpMode requestedPump = PumpMode::OFF;
    PumpMode actualPump = PumpMode::OFF;

    FaultCode fault = FaultCode::NONE;

    bool lockedOut = false;

    bool heaterSafetyContactor = false;
    bool heaterSSR = false;

    bool uv = false;
    bool light = false;

    float waterTemperature = NAN;
    float tempDifference = NAN;

    float heaterDemand = 0.0f;

    uint32_t pumpLowStartedMs = 0;
    uint32_t heaterStartedMs = 0;
    uint32_t heatPermissionStartedMs = 0;
    bool thermostatHeating = false;
    bool heatQualificationPassed = false; // NOT implemented/commissioned
    bool unexpectedRiseWarning = false;

    uint32_t tempDisagreementStartedMs = 0;

    uint32_t lastControlMs = 0;

    // V0.9
    bool waterHot = false;            // ambient over-temperature warning (auto-clears)
    bool calibrationInhibit = false;  // sensors disagree >= 0.5 C at rest
    bool supplySag = false;           // 24 V monitor (if fitted)
};

SpaRuntime spa;
uint32_t pumpTransitionStartMs = 0;
bool pumpTransitionPending = false;
bool adcAvailable = false;

void logEvent(const String &message);
void requestPumpMode(PumpMode mode);

// V0.6: SINGLE LOW-CIRCULATION ARBITER. Every LOW minute counts towards
// filtration, irrespective of whether the heater is energised.
static constexpr uint32_t DAILY_FILTER_TARGET_SEC = 4UL * 3600UL;
static constexpr uint32_t AFTER_USE_SEC = 60UL * 60UL;
static constexpr uint32_t READY_TIMEOUT_MS = 2UL * 3600UL * 1000UL;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 30UL * 60UL * 1000UL;
static constexpr uint32_t SAMPLE_RUN_MS = 2UL * 60UL * 1000UL;
static constexpr uint32_t HEAT_PUMP_RUNON_MS = 30UL * 1000UL;
static constexpr uint32_t JETS_TIMEOUT_MS = 20UL * 60UL * 1000UL;
static constexpr uint32_t FILTER_CLEAN_SEC = 56UL * 3600UL;
static constexpr uint32_t FILTER_CLEAN_AGE_DAYS = 14UL;
static constexpr float STANDBY_HYSTERESIS_C = 3.0f;
static constexpr float READY_HYSTERESIS_C = 1.0f;

// ============================================================
// V0.9 CONTROL ADDITIONS - constants
// ============================================================
static constexpr uint32_t MANUAL_LOW_TIMEOUT_MS = 20UL * 60UL * 1000UL;   // manual LOW auto-off
static constexpr float    ECONOMY_FLOOR_C = 35.0f;                        // solar mode, outside window
static constexpr float    ECONOMY_FLOOR_HYST_C = 1.0f;
static constexpr uint32_t WATER_SETTLE_MS = SPA_BENCH_DEMO ? 5000UL : 60000UL; // circulation before a reading counts
static constexpr uint32_t PRIMING_MS = 4UL * 60UL * 1000UL;               // power-on priming, heater locked
static constexpr float    FREEZE_ON_C = 5.0f, FREEZE_OFF_C = 8.0f;
static constexpr bool     FREEZE_BLIND_RUN = true;                        // both sensors invalid -> 15 min/hour LOW
static constexpr uint32_t FREEZE_BLIND_PERIOD_MS = 60UL * 60UL * 1000UL, FREEZE_BLIND_RUN_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t INPUT_LOCKOUT_MS = 2000;                        // jets/light start changes; stops exempt
static constexpr uint32_t UV_SETTLE_MS = 1000;                            // UV after pump (either speed) runs
static constexpr uint32_t LIGHT_TIMEOUT_MS = 40UL * 60UL * 1000UL;
static constexpr uint8_t  LIGHT_PROGRAM_COUNT = 7;                        // set from bench measurement
static constexpr uint32_t LIGHT_STEP_OFF_MS = 500, LIGHT_STEP_ON_MS = 500;
static constexpr uint32_t LIGHT_RESET_OFF_MS = 10000;                     // off this long -> light restarts at program 1
static constexpr uint32_t FLOW_RETRY_DELAY_MS = 15UL * 60UL * 1000UL;     // OEM-style 'dr' wait
static constexpr uint32_t FLOW_PURGE_MS = 30000;                          // HIGH purge before each retry
static constexpr uint32_t FLOW_TRIP_WINDOW_MS = 24UL * 3600UL * 1000UL;
static constexpr uint8_t  FLOW_TRIPS_TO_LATCH = 3;
static constexpr uint32_t AMBIENT_HOT_SSR_OFF_MS = 10UL * 60UL * 1000UL;  // SSR off this long -> heat is ambient
static constexpr float    WATER_HOT_CLEAR_C = 41.0f;
static constexpr float    CAL_MISMATCH_C = 0.5f, CAL_MISMATCH_CLEAR_C = 0.4f;
static constexpr uint32_t CAL_SETTLE_MS = 120000, CAL_PERSIST_MS = 60000;
static constexpr float    HEATER_SENSOR_CAP_C = USER_MAX_SETPOINT_C + 1.0f; // either sensor -> heater off
static constexpr uint32_t SSR_PROOF_MS = SPA_BENCH_DEMO ? 1000UL : 30000UL; // contactor on, SSR off, no rise
static constexpr float    SSR_PROOF_RISE_C = 0.3f;
static constexpr uint32_t SSR_PROOF_PERSIST_MS = 5000;                    // rise must persist (noise immunity)
static constexpr uint32_t HEAT_RESPONSE_MS = 120000;                      // SSR on this long with flow...
static constexpr float    HEAT_RESPONSE_MIN_DIFF_C = 0.25f;               // ...must show an outlet/inlet difference
static constexpr uint32_t RISE_PERSIST_SSR_MS = SPA_BENCH_DEMO ? 0UL : 3000UL;   // noise immunity for rate checks
static constexpr uint32_t RISE_PERSIST_FLOW_MS = 0UL;   // flow loss shows as a brief rate split: no persistence
static constexpr uint32_t SENSOR_RECOVER_MS = 60000;                      // transient sensor fault auto-clears
static constexpr uint8_t  SENSOR_FAULTS_TO_LATCH = 3;                     // ...unless 3 in 24 h

// All times are commanded LOW-pump time until positive flow feedback exists.
uint32_t dailyLowSec = 0, sinceCleanLowSec = 0;
uint32_t lastRuntimeMs = 0, lastRuntimePersistMs = 0;
uint32_t afterUseRemainingMs = 0, lastAfterUseTickMs = 0;
uint32_t lastInteractionMs = 0, jetsStartMs = 0;
uint32_t lastSampleMs = 0, sampleStartMs = 0;
uint32_t postHeatRunonStartedMs = 0;
bool postHeatRunonActive = false;
uint32_t cleanEpoch = 0;
int lastDayKey = -1;
bool readyMode = false, jetsRunningPreviously = false;
bool manualPumpRequest = false, heatPumpNeeded = false;
bool filteredTimeValid = false;
#if SPA_BENCH_DEMO
int demoMinuteOfDay = -1, demoDay = 1;
uint32_t demoClockBaseMs = 0;
#endif

// ============================================================
// V0.9 CONTROL ADDITIONS - state
// ============================================================
float lastCirculatingWaterC = NAN;      // last reading taken with water circulating
uint32_t lastCirculatingWaterMs = 0;
bool circulatingWaterValid = false;
uint32_t pumpRunStartMs = 0, pumpStoppedAtMs = 0, lastPumpChangeMs = 0;
bool pumpWasRunning = false;
PumpMode lastActualPumpSeen = PumpMode::OFF;
uint32_t primingUntilMs = 0; bool primingActive = false; uint32_t v09BootMs = 0;
bool freezeActive = false; uint32_t blindSinceMs = 0;
uint32_t manualLowStartMs = 0;
bool floorHeatNeeded = false;
uint32_t pumpLockMs = 0, lightLockMs = 0; bool pumpLockSeen = false, lightLockSeen = false;
bool lightOn = false; uint8_t lightProgram = 1, lightWantedProgram = 1;
uint32_t lightOffSinceMs = 0, lightOnSinceMs = 0;
struct LightStep { uint32_t at; bool on; };
LightStep lightQueue[2 * LIGHT_PROGRAM_COUNT + 2]; uint8_t lightQueueLen = 0;
uint32_t flowTripAt[FLOW_TRIPS_TO_LATCH] = {}; uint8_t flowTripCount = 0;
bool flowRetryPending = false; uint32_t flowRetryAtMs = 0;
bool purgeActive = false; uint32_t purgeUntilMs = 0;
uint32_t calMismatchSinceMs = 0, calClearSinceMs = 0;
float ssrProofBaseDiff = NAN; uint32_t ssrProofRiseSinceMs = 0; uint32_t unexpectedRiseSinceMs = 0, flowRiseSinceMs = 0;
uint32_t sensorFaultAt[SENSOR_FAULTS_TO_LATCH] = {}; uint8_t sensorFaultCount = 0; uint32_t sensorHealthySinceMs = 0;
float sensorSupplyV = 3.300f;           // ratiometric divider supply (measured if fitted)
float supplyVolts = NAN; uint32_t supplyOkSinceMs = 0;
void flowRetryTick(uint32_t now);
void processLight(uint32_t now);
void processSensorAgreement(uint32_t now);
void lightSetOn(bool on, uint32_t now);

bool spaClock(int &dayKey, int &minuteOfDay);
void noteInteraction();
void saveRuntime();
void loadRuntime();
uint32_t cleanAgeDays();
void processCombinedCirculation();
void relayWrite(uint8_t pin, bool on);
void heaterSSRWrite(bool on);
void heaterSafetyWrite(bool on);
void allHeaterOutputsOff();
void logEvent(const String &message);
String eventsText();
void raiseFault(FaultCode code, bool lockout);
float voltageToResistance(float voltage);
float resistanceToTemperatureC(
    float resistance,
    float R25,
    float beta
);
SensorStatus classifySensorVoltage(float voltage);
void updateTemperatureChannel(
    TemperatureChannel &sensor,
    int16_t adcRaw,
    float R25,
    float beta,
    float offset
);
void injectSimulatedTemp(TemperatureChannel &s, float reading, bool failed);
void readTemperatureSensors();
bool temperatureSystemHealthy();
bool hardwareHighLimitOK();
void pumpOutputsOff();
void requestPumpMode(PumpMode mode);
void processPumpRequest();
bool heaterPermitted();
bool heaterQualificationReady();
void monitorHeaterTemperatures();
void processHeater();
void processUV();
void updateSpaState();
void initialiseOutputs();
void loadConfiguration();
void printStatus();
bool schedulePermitsHeating();
String statusJSON();
bool webAuth();
void setupWeb();
void processWeb();
String executeCommand(String line);
void processSerialCommands();
void setup();
void loop();

bool persistConfiguration();
bool persistSafety();
void loadSafety();
void forceShutdown();
void storageFailure();
bool setServiceMode(bool enabled);
void checkSensorFreshness();
#include "SpaPersistence.h"

void forceShutdown() {
    allHeaterOutputsOff();
    pumpOutputsOff();
    spa.requestedPump = PumpMode::OFF;
    pumpTransitionPending = false;
    manualPumpRequest = heatPumpNeeded = false;
    spa.thermostatHeating = spa.heatQualificationPassed = false;
    spa.heaterDemand = 0;
    spa.uv = spa.light = false;
    relayWrite(PIN_RELAY_UV, false);
    if (lightOn) lightOffSinceMs = millis();
    lightOn = false; lightQueueLen = 0;
    relayWrite(PIN_RELAY_LIGHT, false);
    purgeActive = false; primingActive = false;
}
void storageFailure() {
    forceShutdown();
    storageHealthy = configurationValid = false;
    serviceMode = true;
    spa.fault = FaultCode::CONFIG_CORRUPT;
    spa.lockedOut = true;
    spa.state = SpaState::LOCKOUT;
    logEvent("SETTINGS IO/INTEGRITY FAILURE; SERVICE REQUIRED");
}
bool setServiceMode(bool enabled) {
    // Stop first, even when the persistence medium has failed.
    forceShutdown();
    readyMode = false;
    jetsRunningPreviously = false; // preserve saved after-use remainder, do not create a new request
    postHeatRunonActive = false;
    sampleStartMs = 0;
    if (!enabled && (!storageHealthy || !configurationValid || spa.fault != FaultCode::NONE)) return false;
    serviceMode = enabled;
    if (!persistSafety()) return false;
    logEvent(enabled ? "SERVICE ON: ALL OUTPUTS OFF" : "SERVICE OFF: AUTOMATIC CONTROL RESUMES");
    updateSpaState();
    return true;
}
void checkSensorFreshness() {
    if (sensorsAcquired && (millis() - tempA.lastUpdateMs > SENSOR_STALE_MS ||
        millis() - tempB.lastUpdateMs > SENSOR_STALE_MS))
        raiseFault(FaultCode::SENSOR_STALE, true);
}
void setupWatchdog() {
    esp_task_wdt_config_t wdt{};
    wdt.timeout_ms = WATCHDOG_TIMEOUT_MS;
    wdt.idle_core_mask = 0;
    wdt.trigger_panic = true;
    esp_err_t result = esp_task_wdt_init(&wdt);
    if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_reconfigure(&wdt);
    if (result != ESP_OK || esp_task_wdt_add_user("spa-control", &controlWatchdog) != ESP_OK) {
        controlWatchdog = nullptr;
        raiseFault(FaultCode::WATCHDOG_RESET, true);
        serviceMode = true;
        persistSafety();
    }
}

bool spaClock(int &dayKey, int &minuteOfDay) {
#if SPA_BENCH_DEMO
    if (demoMinuteOfDay >= 0) {
        uint32_t elapsed = (millis() - demoClockBaseMs) / 60000UL;
        uint32_t minutes = (uint32_t)demoMinuteOfDay + elapsed;
        dayKey = demoDay + minutes / 1440UL;
        minuteOfDay = minutes % 1440UL;
        return true;
    }
#endif
    struct tm t;
    if (!getLocalTime(&t, 0)) return false;
    dayKey = (t.tm_year + 1900) * 400 + t.tm_yday;
    minuteOfDay = t.tm_hour * 60 + t.tm_min;
    return true;
}
void noteInteraction() {
    readyMode = true;
    lastInteractionMs = millis();
}
void saveRuntime() { persistRuntime(); }
void loadRuntime() {
    restoreRuntime();
    lastRuntimeMs = lastRuntimePersistMs = lastAfterUseTickMs = millis();
}
uint32_t cleanAgeDays() {
    if (!cleanEpoch) return 0;
    time_t now = time(nullptr);
    if (now < (time_t)cleanEpoch) return 0;
    return (uint32_t)((now - cleanEpoch) / 86400);
}

// ============================================================
// V0.9 CONTROL ADDITIONS - pump tracking, readings, freeze
// ============================================================
uint32_t pumpRunningMs(uint32_t now) {
    return (spa.actualPump != PumpMode::OFF && pumpRunStartMs) ? now - pumpRunStartMs : 0;
}
// Track continuous circulation (either speed; a changeover dead-time does not
// restart the clock) and capture the last reading taken with water moving.
void trackPumpRun(uint32_t now) {
    bool running = spa.actualPump != PumpMode::OFF;
    if (running && !pumpWasRunning) {
        if (!pumpRunStartMs || now - pumpStoppedAtMs > 5000UL) pumpRunStartMs = now;
        lastPumpChangeMs = now;
    } else if (!running && pumpWasRunning) {
        pumpStoppedAtMs = now; lastPumpChangeMs = now;
    } else if (running && spa.actualPump != lastActualPumpSeen) {
        lastPumpChangeMs = now;
    }
    if (!running && pumpRunStartMs && now - pumpStoppedAtMs > 5000UL) pumpRunStartMs = 0;
    pumpWasRunning = running; lastActualPumpSeen = spa.actualPump;
    if (running && pumpRunningMs(now) >= WATER_SETTLE_MS &&
        tempA.status == SensorStatus::OK && tempB.status == SensorStatus::OK &&
        isfinite(spa.waterTemperature)) {
        lastCirculatingWaterC = spa.waterTemperature;
        lastCirculatingWaterMs = now;
        circulatingWaterValid = true;
    }
}
bool readingFresh(uint32_t now) {
    return circulatingWaterValid && now - lastCirculatingWaterMs < SAMPLE_INTERVAL_MS;
}
bool freezeOverrideAllowed(FaultCode c) {
    switch (c) {
        case FaultCode::RAPID_TEMP_RISE: case FaultCode::FLOW_SUSPECT: case FaultCode::PUMP_INTERLOCK:
        case FaultCode::LOW_CONTACTOR_FAULT: case FaultCode::HIGH_CONTACTOR_FAULT:
        case FaultCode::CONFIG_CORRUPT: case FaultCode::WATCHDOG_RESET: case FaultCode::POWER_FAULT:
            return false;
        default: return true;
    }
}
static constexpr uint32_t FREEZE_BLIND_DELAY_MS = 10UL * 60UL * 1000UL;
bool blindFreezeRun(uint32_t now) {
    if (!FREEZE_BLIND_RUN || !blindSinceMs || now - blindSinceMs < FREEZE_BLIND_DELAY_MS) return false;
    return ((now - blindSinceMs - FREEZE_BLIND_DELAY_MS) % FREEZE_BLIND_PERIOD_MS) < FREEZE_BLIND_RUN_MS;
}
// Freeze protection may run pump LOW even during a non-flow fault/lockout.
bool freezeOverrideActive() {
    uint32_t now = millis();
    return !serviceMode && storageHealthy && (freezeActive || blindFreezeRun(now)) &&
        (spa.fault == FaultCode::NONE || freezeOverrideAllowed(spa.fault));
}
void processFreeze(uint32_t now) {
    bool aOK = tempA.status == SensorStatus::OK && isfinite(tempA.filteredC);
    bool bOK = tempB.status == SensorStatus::OK && isfinite(tempB.filteredC);
    if (!aOK && !bOK) { if (!blindSinceMs) blindSinceMs = now ? now : 1; }
    else blindSinceMs = 0;
    // Either sensor below 5 C triggers: never averaged, so one failed-high probe cannot block it.
    float coldest = aOK && bOK ? fmin(tempA.filteredC, tempB.filteredC) : aOK ? tempA.filteredC : bOK ? tempB.filteredC : NAN;
    float warmestNeeded = coldest; // exit requires every valid sensor >= 8 C
    if (isfinite(coldest) && coldest < FREEZE_ON_C && !freezeActive) {
        freezeActive = true; logEvent("FREEZE PROTECT: pump LOW");
    }
    if (freezeActive && isfinite(warmestNeeded) && warmestNeeded >= FREEZE_OFF_C &&
        pumpRunningMs(now) >= WATER_SETTLE_MS) {
        freezeActive = false; logEvent("FREEZE PROTECT CLEARED");
    }
}
bool solarWindowOpen(int minute) {
    return solarStart != solarEnd && (solarStart < solarEnd ?
        minute >= (int)solarStart * 60 && minute < (int)solarEnd * 60 :
        minute >= (int)solarStart * 60 || minute < (int)solarEnd * 60);
}

bool transientSensorFault(FaultCode c) {
    return c == FaultCode::TEMP_A_OPEN || c == FaultCode::TEMP_A_SHORT || c == FaultCode::TEMP_B_OPEN ||
        c == FaultCode::TEMP_B_SHORT || c == FaultCode::TEMP_A_INVALID || c == FaultCode::TEMP_B_INVALID;
}
// V0.9.1: a brief sensor dropout (loose connector, splash) clears itself after the sensor
// has read healthy for 60 s; the third occurrence within 24 h latches for service.
void processSensorFaultRecovery(uint32_t now) {
    static FaultCode lastSeen = FaultCode::NONE;
    if (spa.lockedOut || !transientSensorFault(spa.fault)) { lastSeen = spa.fault; sensorHealthySinceMs = 0; return; }
    if (lastSeen != spa.fault) {   // new occurrence
        uint8_t n = 0;
        for (uint8_t i = 0; i < sensorFaultCount; ++i) if (now - sensorFaultAt[i] < FLOW_TRIP_WINDOW_MS) sensorFaultAt[n++] = sensorFaultAt[i];
        sensorFaultCount = n;
        if (sensorFaultCount < SENSOR_FAULTS_TO_LATCH) sensorFaultAt[sensorFaultCount++] = now;
        lastSeen = spa.fault;
        if (sensorFaultCount >= SENSOR_FAULTS_TO_LATCH) {
            spa.lockedOut = true; persistSafety();
            logEvent("SENSOR FAULT 3 TIMES IN 24 H: LOCKOUT");
            return;
        }
    }
    bool healthy = tempA.status == SensorStatus::OK && tempB.status == SensorStatus::OK &&
        isfinite(spa.waterTemperature) && fmax(tempA.filteredC, tempB.filteredC) < ABSOLUTE_TEMP_TRIP_C;
    if (!healthy) { sensorHealthySinceMs = 0; return; }
    if (!sensorHealthySinceMs) sensorHealthySinceMs = now ? now : 1;
    if (now - sensorHealthySinceMs >= SENSOR_RECOVER_MS) {
        logEvent("SENSOR FAULT CLEARED AUTOMATICALLY (healthy 60 s)");
        spa.fault = FaultCode::NONE; lastSeen = FaultCode::NONE; sensorHealthySinceMs = 0; persistSafety();
    }
}

void processCombinedCirculation() {
    const uint32_t now = millis();
    processSensorFaultRecovery(now);
    trackPumpRun(now);
    processFreeze(now);
    if (primingActive && (int32_t)(now - primingUntilMs) >= 0) {
        primingActive = false; logEvent("PRIMING COMPLETE");
    }
    flowRetryTick(now);
    if (!lastRuntimeMs) lastRuntimeMs = now;
    const uint32_t deltaMs = now - lastRuntimeMs;
    lastRuntimeMs = now;
    static uint32_t runtimeRemainderMs = 0;
    if (spa.actualPump == PumpMode::LOW_SPEED && !spa.lockedOut && !serviceMode) {
        runtimeRemainderMs += deltaMs;
        uint32_t seconds = runtimeRemainderMs / 1000UL;
        runtimeRemainderMs %= 1000UL;
        dailyLowSec = seconds > UINT32_MAX - dailyLowSec ? UINT32_MAX : dailyLowSec + seconds;
        sinceCleanLowSec = seconds > UINT32_MAX - sinceCleanLowSec ? UINT32_MAX : sinceCleanLowSec + seconds;
    } // Preserve subsecond credit across pauses.

    int day, minute;
    filteredTimeValid = spaClock(day, minute);
    if (filteredTimeValid && (lastDayKey < 0 || day > lastDayKey)) {
        dailyLowSec = 0;
        lastDayKey = day;
        logEvent("NEW FILTER DAY");
    }
    if (now - lastRuntimePersistMs >= 15UL * 60000UL) {
        saveRuntime(); lastRuntimePersistMs = now;
    }
    if (readyMode && now - lastInteractionMs >= READY_TIMEOUT_MS)
        readyMode = false;

    // After-use clock counts ONLY actual LOW circulation. Jets or fault pause it.
    uint32_t elapsedAfter = now - lastAfterUseTickMs;
    lastAfterUseTickMs = now;
    if (afterUseRemainingMs && spa.actualPump == PumpMode::LOW_SPEED &&
        !spa.lockedOut && spa.fault == FaultCode::NONE) {
        afterUseRemainingMs = elapsedAfter >= afterUseRemainingMs ?
            0 : afterUseRemainingMs - elapsedAfter;
    }

    if (serviceMode) { forceShutdown(); return; }

    if (spa.actualPump == PumpMode::HIGH_SPEED && !jetsRunningPreviously && !purgeActive) {
        jetsStartMs = now; jetsRunningPreviously = true;
        // Persist a conservative full after-use request in case jets lose power.
        afterUseRemainingMs = AFTER_USE_SEC * 1000UL;
        saveRuntime();
    }
    if (spa.actualPump != PumpMode::HIGH_SPEED && jetsRunningPreviously) {
        jetsRunningPreviously = false;
        afterUseRemainingMs = AFTER_USE_SEC * 1000UL;
        saveRuntime();
        logEvent("AFTER USE CIRCULATION REQUESTED");
    }
    if (spa.actualPump == PumpMode::HIGH_SPEED &&
        now - jetsStartMs >= JETS_TIMEOUT_MS) {
        requestPumpMode(PumpMode::OFF);
        logEvent("JETS AUTO TIMEOUT");
    }
    // V0.9: OEM-style 30 s HIGH purge before a no-flow retry.
    if (purgeActive) {
        if ((int32_t)(now - purgeUntilMs) < 0 && !spa.lockedOut && spa.fault == FaultCode::NONE) {
            if (spa.requestedPump != PumpMode::HIGH_SPEED) requestPumpMode(PumpMode::HIGH_SPEED);
            return;
        }
        purgeActive = false;
        if (spa.requestedPump == PumpMode::HIGH_SPEED) requestPumpMode(PumpMode::OFF);
        logEvent("FLOW PURGE COMPLETE");
    }
    // V0.9: manual LOW auto-off.
    if (manualPumpRequest && now - manualLowStartMs >= MANUAL_LOW_TIMEOUT_MS) {
        manualPumpRequest = false; logEvent("MANUAL LOW AUTO TIMEOUT");
    }
    // A jets request takes precedence over every LOW demand.
    if (spa.requestedPump == PumpMode::HIGH_SPEED && !spa.lockedOut && spa.fault == FaultCode::NONE) return;

    // Explicit LOW selection persists until OFF, HIGH, or restart.
    bool authorisedWindow = filteredTimeValid &&
        solarStart != solarEnd && (solarStart < solarEnd ?
        minute >= (int)solarStart * 60 && minute < (int)solarEnd * 60 :
        minute >= (int)solarStart * 60 || minute < (int)solarEnd * 60);
    bool heatingAvailable = heatMode == 2 || (heatMode == 1 && authorisedWindow);
    // V0.9: temperature sampling. Stagnant probes are not water temperature, so a
    // reading only counts after WATER_SETTLE_MS of circulation (either speed).
    // Polls run in every heating mode so displays and the economy floor stay current;
    // when heating is available and no fresh reading exists, poll immediately.
    bool fresh = readingFresh(now);
    bool pumpIdle = spa.actualPump == PumpMode::OFF && spa.requestedPump == PumpMode::OFF;
    bool pollDue = !fresh && (now - (circulatingWaterValid ? lastCirculatingWaterMs : v09BootMs) >= SAMPLE_INTERVAL_MS);
    bool demandPoll = heatingAvailable && !fresh && (!lastSampleMs || now - lastSampleMs >= SAMPLE_INTERVAL_MS);
    if (pumpIdle && (pollDue || demandPoll) && (!lastSampleMs || now - lastSampleMs >= SAMPLE_RUN_MS)) {
        sampleStartMs = now ? now : 1;
        lastSampleMs = sampleStartMs;
    }
    // A poll ends at SAMPLE_RUN_MS, or earlier once a settled reading has been averaged.
    bool sampleDone = circulatingWaterValid && lastCirculatingWaterMs - sampleStartMs >= WATER_SETTLE_MS + 5000UL &&
        lastCirculatingWaterMs - sampleStartMs < SAMPLE_RUN_MS + WATER_SETTLE_MS;
    bool sample = sampleStartMs && now - sampleStartMs < SAMPLE_RUN_MS && !sampleDone;
    float w = fresh ? lastCirculatingWaterC : NAN;

    // Demand is computed only from a validated reading; it never grants
    // heater permission. Existing heater safety state machine still gates CH1/3.
    float restartAt = config.setpointC -
        (readyMode ? READY_HYSTERESIS_C : STANDBY_HYSTERESIS_C);
    if (heatingAvailable && isfinite(w)) {
        if (w <= restartAt) heatPumpNeeded = true;
        if (w >= config.setpointC) heatPumpNeeded = false;
    } else if (!heatingAvailable || !spa.heaterSSR) heatPumpNeeded = false;
    if (!heatingAvailable) heatPumpNeeded = false;

    // V0.9: solar mode keeps at least ECONOMY_FLOOR_C outside the window.
    float floorTarget = fmin(ECONOMY_FLOOR_C, config.setpointC);
    if (heatMode == 1 && !authorisedWindow && isfinite(w)) {
        if (w <= floorTarget - ECONOMY_FLOOR_HYST_C) floorHeatNeeded = true;
        if (w >= floorTarget) floorHeatNeeded = false;
    } else if (heatMode != 1 || authorisedWindow) floorHeatNeeded = false;

    // Preferred windows 11-13 and 15-16; use 13-15 when required to
    // meet target. Never extend past the permitted window automatically.
    bool preferred = authorisedWindow &&
        ((minute >= 11 * 60 && minute < 13 * 60) ||
         (minute >= 15 * 60 && minute < 16 * 60));
    uint32_t secondsLeftInWindow = authorisedWindow ?
        (uint32_t)((solarEnd * 60 - minute + 1440) % 1440) * 60UL : 0;
    bool catchup = authorisedWindow && dailyLowSec < DAILY_FILTER_TARGET_SEC &&
        (DAILY_FILTER_TARGET_SEC - dailyLowSec >= secondsLeftInWindow);
    bool filtrationRequired = (preferred || catchup) &&
        dailyLowSec < DAILY_FILTER_TARGET_SEC;
    // When heating ceases, honour the short circulation run-on.
    static bool wasHeaterOn = false;
    if (wasHeaterOn && !spa.heaterSSR) {
        postHeatRunonStartedMs = now;
        postHeatRunonActive = true;
    }
    wasHeaterOn = spa.heaterSSR;
    if (postHeatRunonActive && now - postHeatRunonStartedMs >= HEAT_PUMP_RUNON_MS)
        postHeatRunonActive = false;
    bool runon = postHeatRunonActive;
    bool wantLow = manualPumpRequest || filtrationRequired ||
        heatPumpNeeded || sample || runon || spa.heaterSSR || afterUseRemainingMs > 0 ||
        primingActive || floorHeatNeeded || freezeActive || blindFreezeRun(now);
    if (spa.lockedOut || spa.fault != FaultCode::NONE) wantLow = freezeOverrideActive();
    PumpMode desired = wantLow ? PumpMode::LOW_SPEED : PumpMode::OFF;
    if (spa.requestedPump != desired) requestPumpMode(desired);
}

// ============================================================
// SAFE OUTPUT HELPERS
// ============================================================

void relayWrite(uint8_t pin, bool on)
{
    // Neither shipping build can energise CH1/CH3. Demo exposes logical
    // heater state in status, while these physical pins are always OFF.
    if (serviceMode || !storageHealthy || pin == PIN_RELAY_HEAT_SAFE || pin == PIN_RELAY_HEAT_SSR) on = false;
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}

uint32_t lastSSRoffMs = 0;
void heaterSSRWrite(bool on)
{
    if (spa.heaterSSR && !on) lastSSRoffMs = millis();
    relayWrite(PIN_RELAY_HEAT_SSR, on);
    spa.heaterSSR = on;
}

void heaterSafetyWrite(bool on)
{
    relayWrite(PIN_RELAY_HEAT_SAFE, on);
    spa.heaterSafetyContactor = on;
}

void allHeaterOutputsOff()
{
    heaterSSRWrite(false);
    heaterSafetyWrite(false);
}

// ============================================================
// V0.9 CONTROL ADDITIONS - light, input lockout, no-flow retry, agreement
// ============================================================
bool inputLocked(bool pump) {
    uint32_t t = pump ? pumpLockMs : lightLockMs; bool seen = pump ? pumpLockSeen : lightLockSeen;
    return seen && millis() - t < INPUT_LOCKOUT_MS;
}
void noteInputChange(bool pump) {
    if (pump) { pumpLockMs = millis(); pumpLockSeen = true; }
    else { lightLockMs = millis(); lightLockSeen = true; }
}
// 2-wire RGB light: colour steps by brief power interruptions. Every switch-on
// starts from a known program (light off >= LIGHT_RESET_OFF_MS -> program 1),
// then steps to the last chosen program. After boot the light is OFF and is
// assumed to restart at program 1.
void lightQueueSteps(uint32_t t, uint8_t steps) {
    for (uint8_t i = 0; i < steps && lightQueueLen + 2 <= (uint8_t)(sizeof(lightQueue) / sizeof(lightQueue[0])); ++i) {
        lightQueue[lightQueueLen++] = {t, false}; t += LIGHT_STEP_OFF_MS;
        lightQueue[lightQueueLen++] = {t, true};  t += LIGHT_STEP_ON_MS;
    }
}
void processLight(uint32_t now) {
    while (lightQueueLen && (int32_t)(now - lightQueue[0].at) >= 0) {
        bool on = lightQueue[0].on && lightOn;
        relayWrite(PIN_RELAY_LIGHT, on);
        for (uint8_t i = 1; i < lightQueueLen; ++i) lightQueue[i - 1] = lightQueue[i];
        --lightQueueLen;
    }
    if (lightOn && now - lightOnSinceMs >= LIGHT_TIMEOUT_MS) {
        lightSetOn(false, now); logEvent("LIGHT AUTO OFF (40 min)");
    }
}
void lightSetOn(bool on, uint32_t now) {
    lightQueueLen = 0;
    if (on) {
        lightOn = spa.light = true; lightOnSinceMs = now;
        uint32_t t = now;
        if (lightOffSinceMs && now - lightOffSinceMs < LIGHT_RESET_OFF_MS)
            t = lightOffSinceMs + LIGHT_RESET_OFF_MS;   // wait out the reset time: known program 1
        lightQueue[lightQueueLen++] = {t, true};
        lightQueueSteps(t + LIGHT_STEP_ON_MS, (lightWantedProgram + LIGHT_PROGRAM_COUNT - 1) % LIGHT_PROGRAM_COUNT);
        lightProgram = lightWantedProgram;
    } else {
        if (lightOn) lightOffSinceMs = now ? now : 1;
        lightOn = spa.light = false;
        relayWrite(PIN_RELAY_LIGHT, false);
    }
    processLight(now);
}
void lightSelectProgram(uint8_t program, uint32_t now) {
    lightWantedProgram = program;
    if (!lightOn || lightQueueLen) return;   // applied at next switch-on
    lightQueueSteps(now, (program + LIGHT_PROGRAM_COUNT - lightProgram) % LIGHT_PROGRAM_COUNT);
    lightProgram = program;
    processLight(now);
}
void flowTrip() {
    uint32_t now = millis(); uint8_t n = 0;
    for (uint8_t i = 0; i < flowTripCount; ++i) if (now - flowTripAt[i] < FLOW_TRIP_WINDOW_MS) flowTripAt[n++] = flowTripAt[i];
    flowTripCount = n;
    if (flowTripCount < FLOW_TRIPS_TO_LATCH) flowTripAt[flowTripCount++] = now;
    bool latch = flowTripCount >= FLOW_TRIPS_TO_LATCH;
    raiseFault(FaultCode::RAPID_TEMP_RISE, latch);
    flowRetryPending = !latch;
    flowRetryAtMs = now + FLOW_RETRY_DELAY_MS;
    logEvent(latch ? String("NO-FLOW 3/3 IN 24 H: LOCKOUT") : "NO-FLOW " + String(flowTripCount) + "/3: retry in 15 min");
}
void flowRetryTick(uint32_t now) {
    if (!flowRetryPending) return;
    if (spa.lockedOut || spa.fault != FaultCode::RAPID_TEMP_RISE || serviceMode) { flowRetryPending = false; return; }
    if ((int32_t)(now - flowRetryAtMs) < 0) return;
    if (tempA.status != SensorStatus::OK || tempB.status != SensorStatus::OK ||
        fmax(tempA.filteredC, tempB.filteredC) >= ABSOLUTE_TEMP_TRIP_C) { flowRetryAtMs = now + 60000UL; return; }
    spa.fault = FaultCode::NONE; flowRetryPending = false; persistSafety();
    purgeActive = true; purgeUntilMs = now + FLOW_PURGE_MS;
    logEvent("NO-FLOW RETRY: 30 s HIGH purge");
}
void processSensorAgreement(uint32_t now) {
    bool ok = tempA.status == SensorStatus::OK && tempB.status == SensorStatus::OK && isfinite(spa.tempDifference);
    bool atRest = ok && pumpRunningMs(now) >= CAL_SETTLE_MS && !spa.heaterSSR &&
        (lastSSRoffMs == 0 || now - lastSSRoffMs >= CAL_SETTLE_MS);
    if (!atRest) { calMismatchSinceMs = calClearSinceMs = 0; return; }
    if (spa.tempDifference >= CAL_MISMATCH_C) {
        calClearSinceMs = 0;
        if (!calMismatchSinceMs) calMismatchSinceMs = now ? now : 1;
        if (!spa.calibrationInhibit && now - calMismatchSinceMs >= CAL_PERSIST_MS) {
            spa.calibrationInhibit = true; logEvent("SENSOR MISMATCH >= 0.5 C AT REST: heating inhibited");
        }
    } else if (spa.tempDifference < CAL_MISMATCH_CLEAR_C) {
        calMismatchSinceMs = 0;
        if (spa.calibrationInhibit) {
            if (!calClearSinceMs) calClearSinceMs = now ? now : 1;
            if (now - calClearSinceMs >= CAL_PERSIST_MS) { spa.calibrationInhibit = false; logEvent("SENSOR MISMATCH CLEARED"); }
        }
    }
}

// Bounded diagnostic event log (RAM only; resets on power loss).
static constexpr uint8_t EVENT_COUNT = 24;
String eventLog[EVENT_COUNT];
uint8_t eventHead = 0, eventSize = 0;
void logEvent(const String &message) {
    eventLog[eventHead] = String(millis()) + "ms " + message;
    eventHead = (eventHead + 1) % EVENT_COUNT;
    if (eventSize < EVENT_COUNT) ++eventSize;
}
String eventsText() {
    String out;
    for (uint8_t i=0; i<eventSize; ++i) {
        uint8_t j=(eventHead + EVENT_COUNT - eventSize+i)%EVENT_COUNT;
        out += eventLog[j]+"\n";
    }
    return out;
}

// ============================================================
// FAULT MANAGEMENT
// ============================================================

void raiseFault(FaultCode code, bool lockout)
{
    bool newFault = (spa.fault == FaultCode::NONE);
    if (newFault) spa.fault = code;

    // Heater dies FIRST; all circulation and sanitiser outputs follow.
    // V0.9: freeze protection keeps pump LOW running through non-flow faults.
    allHeaterOutputsOff();
    if (!(freezeOverrideActive() && freezeOverrideAllowed(code))) {
        pumpOutputsOff();
        spa.requestedPump = PumpMode::OFF;
        pumpTransitionPending = false;
    }
    manualPumpRequest = false;
    heatPumpNeeded = false;
    spa.uv = false;
    relayWrite(PIN_RELAY_UV, false);
    if (newFault) {
        // Store only after physical outputs have been removed.
        persistSafety();
    }
    if (newFault) logEvent("FAULT " + String(static_cast<unsigned>(code)) + (lockout ? " LOCKOUT" : ""));

    spa.heaterDemand = 0.0f;
    spa.thermostatHeating = false;
    spa.heatQualificationPassed = false;

    if (lockout)
    {
        spa.lockedOut = true;
        spa.state = SpaState::LOCKOUT;
    }
    else
    {
        spa.state = SpaState::FAULT;
    }

    Serial.printf(
        "FAULT: code=%u lockout=%s\n",
        static_cast<unsigned>(code),
        lockout ? "YES" : "NO"
    );
}

// ============================================================
// NTC CONVERSION
// ============================================================

float voltageToResistance(float voltage)
{
    if (voltage <= 0.0f || voltage >= sensorSupplyV)
        return NAN;

    /*
       Circuit:

       3.3V
        |
       R_FIXED
        |
        +---- ADC
        |
       NTC
        |
       GND

       Vout = Vcc * Rntc / (Rfixed + Rntc)

       Therefore:

       Rntc = Rfixed * Vout / (Vcc - Vout)
    */

    return R_FIXED * voltage / (sensorSupplyV - voltage);
}

float resistanceToTemperatureC(
    float resistance,
    float R25,
    float beta
)
{
    if (resistance <= 0.0f)
        return NAN;

    float invT =
        (1.0f / T0_KELVIN) +
        (log(resistance / R25) / beta);

    float kelvin = 1.0f / invT;

    return kelvin - 273.15f;
}

// ============================================================
// SENSOR VALIDATION
// ============================================================

SensorStatus classifySensorVoltage(float voltage)
{
    if (voltage <= SENSOR_SHORT_V)
        return SensorStatus::SHORT;

    if (voltage >= (SPA_TEMP_ADC_ADS1115 ? SENSOR_OPEN_V : SENSOR_OPEN_V_ONBOARD))
        return SensorStatus::OPEN;

    return SensorStatus::OK;
}

// ============================================================
// SENSOR UPDATE
// ============================================================

void updateTemperatureChannelVolts(TemperatureChannel &sensor, float voltage, float R25, float beta, float offset);
void updateTemperatureChannel(
    TemperatureChannel &sensor,
    int16_t adcRaw,
    float R25,
    float beta,
    float offset
)
{
    // ADS1115 GAIN_ONE = +/-4.096V
    updateTemperatureChannelVolts(sensor, ads.computeVolts(adcRaw), R25, beta, offset);
}
void updateTemperatureChannelVolts(
    TemperatureChannel &sensor,
    float voltage,
    float R25,
    float beta,
    float offset
)
{
    uint32_t now = millis();

    sensor.voltage = voltage;

    sensor.status =
        classifySensorVoltage(voltage);

    if (sensor.status != SensorStatus::OK)
    {
        sensor.rawC = NAN;
        sensor.previousC = NAN;
        sensor.filteredC = NAN;
        sensor.rateCPerSec = 0.0f;
        sensor.lastUpdateMs = now;
        return;
    }

    float resistance =
        voltageToResistance(voltage);

    sensor.resistance = resistance;

    float temp =
        resistanceToTemperatureC(
            resistance,
            R25,
            beta
        );

    temp += offset;

    if (
        isnan(temp) ||
        temp < SENSOR_MIN_TEMP_C ||
        temp > SENSOR_MAX_TEMP_C
    )
    {
        sensor.status = SensorStatus::INVALID;
        sensor.rawC = NAN;
        sensor.previousC = NAN;
        sensor.filteredC = NAN;
        sensor.rateCPerSec = 0.0f;
        sensor.lastUpdateMs = now;
        return;
    }

    sensor.rawC = temp;

    // Simple EMA.
    // Can later replace with median + EMA.
    constexpr float alpha = 0.20f;

    if (isnan(sensor.filteredC))
    {
        sensor.filteredC = temp;
    }
    else
    {
        sensor.filteredC =
            (alpha * temp) +
            ((1.0f - alpha) * sensor.filteredC);
    }

    // Calculate temperature slope.
    if (
        !isnan(sensor.previousC) &&
        sensor.lastUpdateMs > 0
    )
    {
        float seconds =
            (now - sensor.lastUpdateMs) / 1000.0f;

        if (seconds > 0.0f)
        {
            sensor.rateCPerSec =
                (sensor.filteredC - sensor.previousC)
                / seconds;
        }
    }

    sensor.previousC = sensor.filteredC;
    sensor.lastUpdateMs = now;
}

// ============================================================
// READ BOTH BALBOA M7 SENSORS
// ============================================================

#if SPA_BENCH_DEMO
float demoTempA = 36.0f, demoTempB = 36.0f;
bool demoSensorAFailed = false, demoSensorBFailed = false;
bool demoHighLimitOK = true;
bool demoLightEnabled = false;
void injectSimulatedTemp(TemperatureChannel &s, float reading, bool failed)
{
    uint32_t now = millis();
    if (failed || !isfinite(reading) || reading < SENSOR_MIN_TEMP_C || reading > SENSOR_MAX_TEMP_C) {
        s.status = SensorStatus::INVALID;
        s.rawC = s.filteredC = NAN;
        s.rateCPerSec = 0;
        s.previousC = NAN;
        s.lastUpdateMs = now;
        return;
    }
    s.status = SensorStatus::OK;
    s.rawC = reading;
    // Direct injection enables reproducible fault scenarios; real filtering remains in main build.
    if (isfinite(s.previousC) && s.lastUpdateMs && now > s.lastUpdateMs)
        s.rateCPerSec = (reading - s.previousC) / ((now - s.lastUpdateMs)/1000.0f);
    else s.rateCPerSec = 0;
    s.filteredC = reading;
    s.previousC = reading;
    s.lastUpdateMs = now;
}
#endif

// ADS1X15's convenience reader waits forever for conversion completion.
// Bound that wait and check device ACK before and after each conversion.
bool readADCChecked(uint16_t mux, int16_t &raw) {
    Wire.beginTransmission(0x48);
    if (Wire.endTransmission() != 0) return false;
    ads.startADCReading(mux, false);
    uint32_t started = millis();
    while (!ads.conversionComplete()) {
        if (millis() - started >= 100UL) return false;
        delay(1);
    }
    raw = ads.getLastConversionResults();
    Wire.beginTransmission(0x48);
    return Wire.endTransmission() == 0;
}

void readTemperatureSensors()
{
    sensorsAcquired = true;
#if SPA_BENCH_DEMO
    injectSimulatedTemp(tempA, demoTempA, demoSensorAFailed);
    injectSimulatedTemp(tempB, demoTempB, demoSensorBFailed);
#else
#if SPA_TEMP_ADC_ADS1115
    int16_t rawA = 0, rawB = 0;
    if (RAIL_REF_FITTED) {
        int16_t rawRail = 0;
        if (adcAvailable && readADCChecked(ADS1X15_REG_CONFIG_MUX_SINGLE_2, rawRail)) {
            float v = ads.computeVolts(rawRail);
            sensorSupplyV = (v >= 3.15f && v <= 3.45f) ? v : SENSOR_VCC;
        }
    }
    if (SUPPLY_MON_FITTED) {
        int16_t raw24 = 0;
        if (adcAvailable && readADCChecked(ADS1X15_REG_CONFIG_MUX_SINGLE_3, raw24)) {
            supplyVolts = ads.computeVolts(raw24) * SUPPLY_MON_RATIO;
            uint32_t now = millis();
            if (supplyVolts < SUPPLY_SAG_V) {
                if (!spa.supplySag) logEvent("24 V SUPPLY SAG: heater inhibited");
                spa.supplySag = true; supplyOkSinceMs = 0; allHeaterOutputsOff();
            } else if (spa.supplySag && supplyVolts >= SUPPLY_OK_V) {
                if (!supplyOkSinceMs) supplyOkSinceMs = now ? now : 1;
                if (now - supplyOkSinceMs >= 5000UL) { spa.supplySag = false; logEvent("24 V SUPPLY RECOVERED"); }
            }
        }
    }
    if (!adcAvailable || !readADCChecked(ADS1X15_REG_CONFIG_MUX_SINGLE_0, rawA) ||
        !readADCChecked(ADS1X15_REG_CONFIG_MUX_SINGLE_1, rawB)) {
        tempA.status = tempB.status = SensorStatus::INVALID;
        tempA.rawC = tempB.rawC = tempA.filteredC = tempB.filteredC = NAN;
        spa.waterTemperature = NAN;
        raiseFault(FaultCode::ADC_FAILURE, true);
        return;
    }
    updateTemperatureChannel(
        tempA,
        rawA,
        config.sensorAR25,
        config.sensorABeta,
        config.sensorAOffset
    );

    updateTemperatureChannel(
        tempB,
        rawB,
        config.sensorBR25,
        config.sensorBBeta,
        config.sensorBOffset
    );
#else
    // Onboard ADC1: average many calibrated millivolt readings (ADC2 is unusable with Wi-Fi).
    auto readMv = [](int pin) -> float {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < ONBOARD_ADC_SAMPLES; ++i) sum += analogReadMilliVolts(pin);
        return sum / (ONBOARD_ADC_SAMPLES * 1000.0f);
    };
    if (RAIL_REF_FITTED) {
        float v = readMv(PIN_RAIL_REF_ADC) * 2.0f;   // 10k/10k divider
        sensorSupplyV = (v >= 3.15f && v <= 3.45f) ? v : SENSOR_VCC;
    }
    updateTemperatureChannelVolts(tempA, readMv(PIN_TEMP_A_ADC), config.sensorAR25, config.sensorABeta, config.sensorAOffset);
    updateTemperatureChannelVolts(tempB, readMv(PIN_TEMP_B_ADC), config.sensorBR25, config.sensorBBeta, config.sensorBOffset);
#endif
    #endif
    if (
        tempA.status == SensorStatus::OK &&
        tempB.status == SensorStatus::OK
    )
    {
        // V0.9.1: while the SSR heats, the warmer probe is the heater outlet; the cooler one
        // (inlet, whichever way round they are wired) is the tub temperature.
        spa.waterTemperature = spa.heaterSSR ?
            fmin(tempA.filteredC, tempB.filteredC) :
            (tempA.filteredC + tempB.filteredC) * 0.5f;

        spa.tempDifference =
            fabs(tempA.filteredC - tempB.filteredC);
    }
    else
    {
        spa.waterTemperature = NAN;
        spa.tempDifference = NAN;
    }
}

// ============================================================
// TEMPERATURE FAULT CHECKS
// ============================================================

bool temperatureSystemHealthy()
{
    if (tempA.status == SensorStatus::OPEN)
    {
        raiseFault(FaultCode::TEMP_A_OPEN, false);
        return false;
    }

    if (tempA.status == SensorStatus::SHORT)
    {
        raiseFault(FaultCode::TEMP_A_SHORT, false);
        return false;
    }

    if (tempB.status == SensorStatus::OPEN)
    {
        raiseFault(FaultCode::TEMP_B_OPEN, false);
        return false;
    }

    if (tempB.status == SensorStatus::SHORT)
    {
        raiseFault(FaultCode::TEMP_B_SHORT, false);
        return false;
    }

    if (tempA.status != SensorStatus::OK)
    {
        raiseFault(FaultCode::TEMP_A_INVALID, false);
        return false;
    }

    if (tempB.status != SensorStatus::OK)
    {
        raiseFault(FaultCode::TEMP_B_INVALID, false);
        return false;
    }

    if (!isfinite(tempA.rawC) || !isfinite(tempB.rawC) ||
        !isfinite(tempA.filteredC) || !isfinite(tempB.filteredC) ||
        millis() - tempA.lastUpdateMs > SENSOR_STALE_MS || millis() - tempB.lastUpdateMs > SENSOR_STALE_MS) {
        raiseFault(FaultCode::SENSOR_STALE, true);
        return false;
    }
    if (fmax(tempA.rawC, tempB.rawC) >= ABSOLUTE_TEMP_TRIP_C ||
        fmax(tempA.filteredC, tempB.filteredC) >= ABSOLUTE_TEMP_TRIP_C) {
        // V0.9: heat with the SSR off for >= 10 min (or never on since boot) is
        // ambient/sun/pump heat: warning that clears itself below 41 C, heater inhibited.
        bool ambient = !spa.heaterSSR &&
            (lastSSRoffMs == 0 || millis() - lastSSRoffMs >= AMBIENT_HOT_SSR_OFF_MS);
        if (ambient) {
            if (!spa.waterHot) logEvent("WATER HOT (not heater): heating inhibited; clears below 41 C");
            spa.waterHot = true;
            allHeaterOutputsOff();
            return false;
        }
        raiseFault(FaultCode::OVERTEMP, true);
        return false;
    }
    if (spa.waterHot && fmax(tempA.rawC, tempB.rawC) < WATER_HOT_CLEAR_C &&
        fmax(tempA.filteredC, tempB.filteredC) < WATER_HOT_CLEAR_C) {
        spa.waterHot = false; logEvent("WATER HOT CLEARED");
    }

    uint32_t now = millis();

    if (spa.tempDifference >= TEMP_DIFF_FAULT_C)
    {
        if (spa.tempDisagreementStartedMs == 0)
            spa.tempDisagreementStartedMs = now;

        if (
            now - spa.tempDisagreementStartedMs >=
            TEMP_DIFF_FAULT_DELAY_MS
        )
        {
            raiseFault(
                FaultCode::TEMP_DISAGREE,
                false
            );

            return false;
        }
    }
    else
    {
        spa.tempDisagreementStartedMs = 0;
    }

    return true;
}

// ============================================================
// HIGH LIMIT
// ============================================================

bool hardwareHighLimitOK()
{
#if SPA_BENCH_DEMO
    return demoHighLimitOK;
#endif
    if (PIN_HIGH_LIMIT_OK < 0)
    {
        // Unknown hardware state is NOT proof that a safety circuit is closed.
        return false;
    }

    return digitalRead(PIN_HIGH_LIMIT_OK) == HIGH;
}

// ============================================================
// PUMP OUTPUT CONTROL
// ============================================================

void pumpOutputsOff()
{
    relayWrite(PIN_RELAY_PUMP_LOW, false);
    relayWrite(PIN_RELAY_PUMP_HIGH, false);

    spa.actualPump = PumpMode::OFF;
}

// Non-blocking break-before-make. Neither contactor is commanded on during deadtime.


void requestPumpMode(PumpMode mode)
{
    if (mode != PumpMode::LOW_SPEED) {
        allHeaterOutputsOff();
        spa.thermostatHeating = false;
        spa.heatQualificationPassed = false;
    }
    if (spa.requestedPump != mode) logEvent("PUMP REQUEST " + String((int)mode));
    spa.requestedPump = mode;
}

void processPumpRequest()
{
    bool freezeRun = freezeOverrideActive() && spa.requestedPump == PumpMode::LOW_SPEED;
    if (serviceMode || !storageHealthy || ((spa.lockedOut || spa.fault != FaultCode::NONE) && !freezeRun)) {
        pumpOutputsOff(); pumpTransitionPending = false;
        return;
    }
    if (spa.requestedPump == PumpMode::OFF) {
        allHeaterOutputsOff();
        pumpOutputsOff(); pumpTransitionPending = false;
        return;
    }
    if (pumpTransitionPending) {
        if (millis() - pumpTransitionStartMs < PUMP_CHANGEOVER_DEADTIME_MS)
            return;
        pumpTransitionPending = false;
    } else if (spa.actualPump == spa.requestedPump) {
        return;
    } else {
        allHeaterOutputsOff();
        pumpOutputsOff();
        pumpTransitionStartMs = millis();
        pumpTransitionPending = true;
        spa.heatQualificationPassed = false;
        return;
    }
    if (spa.requestedPump == PumpMode::LOW_SPEED) {
        relayWrite(PIN_RELAY_PUMP_HIGH, false);
        relayWrite(PIN_RELAY_PUMP_LOW, true);
        spa.actualPump = PumpMode::LOW_SPEED;
        spa.pumpLowStartedMs = millis();
    } else if (spa.requestedPump == PumpMode::HIGH_SPEED) {
        allHeaterOutputsOff();
        relayWrite(PIN_RELAY_PUMP_LOW, false);
        relayWrite(PIN_RELAY_PUMP_HIGH, true);
        spa.actualPump = PumpMode::HIGH_SPEED;
    }
}

// ============================================================
// HEATER PERMISSION
// ============================================================

bool schedulePermitsHeating();
bool heaterPermitted()
{
    if (serviceMode || !storageHealthy || (!SPA_BENCH_DEMO && !HEATER_OUTPUTS_COMMISSIONED) || !schedulePermitsHeating()) return false;
    if (spa.lockedOut)
        return false;

    if (spa.fault != FaultCode::NONE)
        return false;

    // V0.9 inhibits: priming, ambient hot water, sensor mismatch at rest, 24 V sag.
    if (primingActive || spa.waterHot || spa.calibrationInhibit || spa.supplySag)
        return false;

    if (!temperatureSystemHealthy())
        return false;

    // V0.9: either sensor at setpoint ceiling + 1 C removes heat.
    if (fmax(tempA.filteredC, tempB.filteredC) >= HEATER_SENSOR_CAP_C)
        return false;

    if (!hardwareHighLimitOK())
    {
        raiseFault(
            FaultCode::HIGH_LIMIT_OPEN,
            true
        );

        return false;
    }

    // HIGH requested must inhibit heating even before contactor changeover.
    if (spa.requestedPump != PumpMode::LOW_SPEED)
        return false;

    // Heater only on LOW circulation.
    if (spa.actualPump != PumpMode::LOW_SPEED)
        return false;

    // Wait for water circulation.
    if (
        millis() - spa.pumpLowStartedMs <
        MIN_CIRCULATION_BEFORE_HEAT_MS
    )
        return false;

    // Sensor disagreement inhibits heating before full fault.
    if (
        spa.tempDifference >= TEMP_DIFF_INHIBIT_C
    )
        return false;

    if (
        tempA.filteredC >= ABSOLUTE_TEMP_TRIP_C ||
        tempB.filteredC >= ABSOLUTE_TEMP_TRIP_C
    )
        return false;

    return true;
}

// ============================================================
// V0.3 M7-INSPIRED CONTROL / THERMOSTAT (BENCH DEVELOPMENT ONLY)
// ============================================================

// Heating is deliberately locked out until the OEM sensor curve, high-limit,
// circulation proving, independent contactor safety and M7 qualification
// algorithm have been separately validated. Temperature-only checks cannot
// conclusively prove water flow or distinguish SSR failure from residual heat.

bool heaterQualificationReady()
{
    if (spa.actualPump != PumpMode::LOW_SPEED ||
        spa.requestedPump != PumpMode::LOW_SPEED)
        return false;
    if (millis() - spa.pumpLowStartedMs < M7_STABILISATION_MS)
        return false;
    if (!temperatureSystemHealthy() ||
        spa.tempDifference >= TEMP_DIFF_INHIBIT_C)
        return false;
    // This is NOT a reproduction of Balboa's certified dry-fire test.
#if SPA_BENCH_DEMO
    // Bench-only qualification of sensor consistency with injected values.
    spa.heatQualificationPassed = demoHighLimitOK;
#endif
    return spa.heatQualificationPassed;
}

void monitorHeaterTemperatures()
{
    if (!temperatureSystemHealthy())
        return;

    // A high temperature is a fault whether or not the SSR is commanded ON.
    if (tempA.filteredC >= ABSOLUTE_TEMP_TRIP_C ||
        tempB.filteredC >= ABSOLUTE_TEMP_TRIP_C)
    {
        raiseFault(FaultCode::OVERTEMP, true);
        return;
    }

    // Diagnostic only: probes cannot establish a failed SSR conclusively.
    // Do not confuse post-heating thermal soak with uncommanded energisation.
    spa.unexpectedRiseWarning = !spa.heaterSSR &&
        spa.heaterSafetyContactor &&
        (tempA.rateCPerSec > MAX_SINGLE_SENSOR_RISE_C_PER_SEC ||
         tempB.rateCPerSec > MAX_SINGLE_SENSOR_RISE_C_PER_SEC);

    // A thermal anomaly suggests unintended heating, but isn't proof of SSR conduction.
    // Avoid immediate false positives from heat soak after switching OFF.
    // V0.9.1: the rise must persist (single noisy ADC steps are not SSR conduction).
    bool unexpectedNow = spa.unexpectedRiseWarning &&
        millis() - lastSSRoffMs > 120000UL &&
        millis() - spa.pumpLowStartedMs > 120000UL;
    if (!unexpectedNow) unexpectedRiseSinceMs = 0;
    else {
        if (!unexpectedRiseSinceMs) unexpectedRiseSinceMs = millis() ? millis() : 1;
        if (millis() - unexpectedRiseSinceMs >= RISE_PERSIST_SSR_MS) {
            raiseFault(FaultCode::HEATER_SSR_SUSPECT, true);
            return;
        }
    }
    if (spa.heaterSSR &&
        millis() - spa.heaterStartedMs >= RATE_MONITOR_DELAY_MS)
    {
        float rateDifference = fabs(tempA.rateCPerSec - tempB.rateCPerSec);
        bool rising = tempA.rateCPerSec > MAX_SINGLE_SENSOR_RISE_C_PER_SEC ||
            tempB.rateCPerSec > MAX_SINGLE_SENSOR_RISE_C_PER_SEC ||
            rateDifference > MAX_SENSOR_RATE_DIFFERENCE_C_SEC;
        if (!rising) flowRiseSinceMs = 0;
        else {
            if (!flowRiseSinceMs) flowRiseSinceMs = millis() ? millis() : 1;
            if (millis() - flowRiseSinceMs >= RISE_PERSIST_FLOW_MS) { flowRiseSinceMs = 0; flowTrip(); } // V0.9: retry; 3 in 24 h latches
        }
    } else flowRiseSinceMs = 0;
#if !SPA_BENCH_DEMO
    // V0.9.1: with water flowing, a working 2 kW element always opens an outlet/inlet
    // difference within two minutes. None means: element open, SSR/contactor/spare-pole
    // not making, or frozen sensor readings. Demo injects temperatures, so Main only.
    if (spa.heaterSSR && spa.actualPump == PumpMode::LOW_SPEED &&
        millis() - spa.heaterStartedMs >= HEAT_RESPONSE_MS &&
        isfinite(spa.tempDifference) && spa.tempDifference < HEAT_RESPONSE_MIN_DIFF_C) {
        logEvent("NO HEAT RESPONSE: check element, SSR, contactor, LOW spare poles, sensors");
        raiseFault(FaultCode::HEATER_CONTACTOR_FAULT, true);
    }
#endif
}

void processHeater()
{
    monitorHeaterTemperatures();
    if (spa.fault != FaultCode::NONE || spa.lockedOut)
    {
        allHeaterOutputsOff();
        spa.thermostatHeating = false;
        spa.heaterDemand = 0.0f;
        return;
    }

    if (!heaterPermitted() || (!HEATER_OUTPUTS_COMMISSIONED && !SPA_BENCH_DEMO) ||
        !heaterQualificationReady())
    {
        allHeaterOutputsOff();
        spa.thermostatHeating = false;
        spa.heaterDemand = 0.0f;
        return;
    }

    // Keep safety contactor ON during permitted normal thermostat OFF cycles.
    // Independent high-limit and flow safety hardware must be installed.
    if (!spa.heaterSafetyContactor)
    {
        heaterSSRWrite(false);
        heaterSafetyWrite(true);
        spa.heatPermissionStartedMs = millis();
        ssrProofBaseDiff = spa.tempDifference; ssrProofRiseSinceMs = 0;
    }
    // V0.9 SSR proof: contactor closed, SSR off -> outlet must not rise.
    if (!spa.heaterSSR && millis() - spa.heatPermissionStartedMs < SSR_PROOF_MS &&
        isfinite(ssrProofBaseDiff) && isfinite(spa.tempDifference) &&
        spa.tempDifference - ssrProofBaseDiff >= SSR_PROOF_RISE_C) {
        if (!ssrProofRiseSinceMs) ssrProofRiseSinceMs = millis() ? millis() : 1;
        if (millis() - ssrProofRiseSinceMs >= (SPA_BENCH_DEMO ? 0UL : SSR_PROOF_PERSIST_MS)) {
            raiseFault(FaultCode::HEATER_SSR_SUSPECT, true);
            return;
        }
    } else ssrProofRiseSinceMs = 0;

    // Use existing calibrated water temperature until lower-probe regulation
    // has been verified with both OEM probes and real circulation.
    const float t = spa.waterTemperature;
    if (!isfinite(t))
    {
        allHeaterOutputsOff();
        spa.thermostatHeating = false;
        return;
    }

    // V0.9: outside the solar window the economy floor is the target.
    int dayKey = 0, minuteOfDay = 0;
    bool floorOnly = heatMode == 1 && !(spaClock(dayKey, minuteOfDay) && solarWindowOpen(minuteOfDay));
    float target = floorOnly ? fmin(ECONOMY_FLOOR_C, config.setpointC) : config.setpointC;
    float band = floorOnly ? ECONOMY_FLOOR_HYST_C : (readyMode ? READY_HYSTERESIS_C : STANDBY_HYSTERESIS_C);
    if (t >= target)
        spa.thermostatHeating = false;
    else if (t <= target - band && fmax(tempA.filteredC, tempB.filteredC) < USER_MAX_SETPOINT_C)
        spa.thermostatHeating = true;

    bool requestHeat = spa.thermostatHeating &&
        millis() - spa.heatPermissionStartedMs >= (SSR_PROOF_MS > HEATER_CONTACTOR_SETTLE_MS ? SSR_PROOF_MS : HEATER_CONTACTOR_SETTLE_MS);
    if (requestHeat && !spa.heaterSSR)
        spa.heaterStartedMs = millis();
    heaterSSRWrite(requestHeat);
    spa.heaterDemand = requestHeat ? 1.0f : 0.0f;
}

// ============================================================
// UV / OZONE
// ============================================================

void processUV()
{
    // V0.9: UV/ozone runs whenever the pump runs (LOW or HIGH), off when stopped
    // or during changeover, after a short settle.
    bool permitted = !serviceMode && storageHealthy &&
        spa.actualPump != PumpMode::OFF && !pumpTransitionPending &&
        millis() - lastPumpChangeMs >= UV_SETTLE_MS &&
        !spa.lockedOut &&
        spa.fault == FaultCode::NONE;

    spa.uv = permitted;

    relayWrite(
        PIN_RELAY_UV,
        spa.uv
    );
}

// ============================================================
// STATE MANAGER
// ============================================================

void updateSpaState()
{
    if (serviceMode) { spa.state = SpaState::SERVICE; return; }
    if (spa.lockedOut)
    {
        spa.state = SpaState::LOCKOUT;
        return;
    }

    if (spa.fault != FaultCode::NONE)
    {
        spa.state = SpaState::FAULT;
        return;
    }

    if (spa.actualPump == PumpMode::HIGH_SPEED)
    {
        spa.state = SpaState::PUMP_HIGH;
        return;
    }

    if (spa.heaterSSR)
    {
        spa.state = SpaState::HEATING;
        return;
    }

    if (spa.actualPump == PumpMode::LOW_SPEED)
    {
        spa.state = SpaState::CIRCULATING;
        return;
    }

    spa.state = SpaState::IDLE;
}

// ============================================================
// SAFE STARTUP
// ============================================================

void initialiseOutputs()
{
    // Set all relay latches OFF before configuring GPIOs as outputs.
    // Physical boot-state and relay polarity still require bench verification.
    digitalWrite(PIN_RELAY_HEAT_SAFE, RELAY_OFF);
    digitalWrite(PIN_RELAY_PUMP_LOW, RELAY_OFF);
    digitalWrite(PIN_RELAY_HEAT_SSR, RELAY_OFF);
    digitalWrite(PIN_RELAY_PUMP_HIGH, RELAY_OFF);
    digitalWrite(PIN_RELAY_UV, RELAY_OFF);
    digitalWrite(PIN_RELAY_LIGHT, RELAY_OFF);

    pinMode(PIN_RELAY_HEAT_SAFE, OUTPUT);
    pinMode(PIN_RELAY_PUMP_LOW, OUTPUT);
    pinMode(PIN_RELAY_HEAT_SSR, OUTPUT);
    pinMode(PIN_RELAY_PUMP_HIGH, OUTPUT);
    pinMode(PIN_RELAY_UV, OUTPUT);
    pinMode(PIN_RELAY_LIGHT, OUTPUT);

    pumpOutputsOff();
    allHeaterOutputsOff();
    relayWrite(PIN_RELAY_UV, false);
    relayWrite(PIN_RELAY_LIGHT, false);
}

// ============================================================
// CONFIG
// ============================================================

void loadConfiguration() { restoreConfiguration(); }

// ============================================================
// DEBUG STATUS
// ============================================================

void printStatus()
{
    Serial.printf(
        "A=%.2fC  B=%.2fC  AVG=%.2fC  dT=%.2fC  "
        "rateA=%.3f  rateB=%.3f  "
        "Pump=%u Heater=%d Demand=%.0f%% State=%u Fault=%u Qual=%d Enabled=%d RiseWarn=%d\n",

        tempA.filteredC,
        tempB.filteredC,
        spa.waterTemperature,
        spa.tempDifference,

        tempA.rateCPerSec,
        tempB.rateCPerSec,

        static_cast<unsigned>(spa.actualPump),
        spa.heaterSSR,

        spa.heaterDemand * 100.0f,

        static_cast<unsigned>(spa.state),
        static_cast<unsigned>(spa.fault),
        spa.heatQualificationPassed,
        HEATER_OUTPUTS_COMMISSIONED,
        spa.unexpectedRiseWarning
    );
}


String executeCommand(String line);
// Local Wi-Fi configuration: edit credentials before flashing. No open AP fallback.
// HTTP Basic Authentication is NOT encrypted: trusted, isolated LAN only.
#ifndef SPA_WIFI_SSID
#define SPA_WIFI_SSID ""
#endif
#ifndef SPA_WIFI_PASS
#define SPA_WIFI_PASS ""
#endif
#ifndef SPA_WEB_PASS
#define SPA_WEB_PASS "CHANGE_THIS_LONG_PASSWORD"
#endif
static const char *WIFI_SSID = SPA_WIFI_SSID;
static const char *WIFI_PASS = SPA_WIFI_PASS;
static const char *WEB_USER = "spa";
static const char *WEB_PASS = SPA_WEB_PASS;
BoundedWebServer web(80);
uint32_t lastWifiAttempt = 0;

bool schedulePermitsHeating() {
    if (heatMode == 0) return false;
    if (heatMode == 2) return true;
    int day, minute;
    // V0.9: outside the window (or with no valid time) only the economy floor may heat.
    if (!spaClock(day, minute) || solarStart == solarEnd) return floorHeatNeeded;
    return solarWindowOpen(minute) || floorHeatNeeded;
}
String statusJSON() {
    String s="{\"demo\":"+String(SPA_BENCH_DEMO?"true":"false");
    s+=",\"tempA\":"+(isfinite(tempA.filteredC)?String(tempA.filteredC,2):String("null"))+",\"tempB\":"+(isfinite(tempB.filteredC)?String(tempB.filteredC,2):String("null"));
    s+=",\"setpoint\":"+String(config.setpointC,1);
    s+=",\"pump\":"+String((int)spa.actualPump);
    s+=",\"requestedPump\":"+String((int)spa.requestedPump);
    s+=",\"heaterPinsInhibited\":true";
    s+=",\"ch1\":"+String(spa.heaterSafetyContactor?"true":"false");
    s+=",\"ch3\":"+String(spa.heaterSSR?"true":"false");
    s+=",\"light\":"+String(spa.light?"true":"false");
    s+=",\"fault\":"+String((int)spa.fault);
    s+=",\"lockout\":"+String(spa.lockedOut?"true":"false");
    s+=",\"heatMode\":"+String(heatMode);
    s+=",\"serviceMode\":"+String(serviceMode?"true":"false");
    s+=",\"settingsHealthy\":"+String(storageHealthy?"true":"false");
    s+=",\"resetReason\":"+String(resetReason);
    s+=",\"ready\":"+String(readyMode?"true":"false");
    s+=",\"dailyLowSeconds\":"+String(dailyLowSec);
    s+=",\"afterUseSeconds\":"+String(afterUseRemainingMs/1000UL);
    s+=",\"filterCleanDue\":"+String((sinceCleanLowSec >= FILTER_CLEAN_SEC || cleanAgeDays() >= FILTER_CLEAN_AGE_DAYS)?"true":"false");
    s+=",\"heatingCommissioned\":"+String(HEATER_OUTPUTS_COMMISSIONED?"true":"false");
    uint32_t nowMs = millis();
    s+=",\"water\":"+(circulatingWaterValid?String(lastCirculatingWaterC,2):String("null"));
    s+=",\"waterAgeSeconds\":"+(circulatingWaterValid?String((nowMs-lastCirculatingWaterMs)/1000UL):String("null"));
    s+=",\"uv\":"+String(spa.uv?"true":"false");
    s+=",\"priming\":"+String(primingActive?"true":"false");
    s+=",\"primingSeconds\":"+String(primingActive?(primingUntilMs-nowMs)/1000UL:0UL);
    s+=",\"freeze\":"+String(freezeActive?"true":"false");
    s+=",\"waterHot\":"+String(spa.waterHot?"true":"false");
    s+=",\"sensorMismatch\":"+String(spa.calibrationInhibit?"true":"false");
    s+=",\"floorHeat\":"+String(floorHeatNeeded?"true":"false");
    s+=",\"flowRetrySeconds\":"+(flowRetryPending?String((flowRetryAtMs-nowMs)/1000UL):String("null"));
    s+=",\"lightProgram\":"+String(lightProgram);
    s+=",\"tempAdc\":\""+String(SPA_TEMP_ADC_ADS1115?"ADS1115":"ONBOARD")+"\"";
    s+="}"; return s;
}
bool webAuth() {
    if (web.authenticate(WEB_USER,WEB_PASS)) return true;
    web.requestAuthentication(); return false;
}
void setupWeb() {
    if (!strlen(WIFI_SSID) || !strcmp(WEB_PASS,"CHANGE_THIS_LONG_PASSWORD")) {
        Serial.println("Wi-Fi disabled: configure SSID and web password"); return;
    }
    WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID,WIFI_PASS); lastWifiAttempt=millis();
    configTzTime("AEST-10AEDT,M10.1.0,M4.1.0/3","pool.ntp.org","time.google.com");
    web.on("/", HTTP_GET, [](){
        if (!webAuth()) return;
        web.send(200,"text/html",
            "<html><meta name='viewport' content='width=device-width'><h2>Spa V0.6</h2>"
            "<p>GET /api/status for JSON status.</p>"
            "<p>POST /api/cmd with form field cmd, e.g. low, high, off, heatmode manual.</p>"
            "<p>Control restricted to authenticated local LAN.</p></html>");
    });
    web.on("/api/status", HTTP_GET, [](){if(!webAuth())return;web.send(200,"application/json",statusJSON());});
    web.on("/api/events", HTTP_GET, [](){if(!webAuth())return;web.send(200,"text/plain",eventsText());});
    web.on("/api/cmd", HTTP_POST, [](){
        if(!webAuth())return;
        if(!web.hasArg("cmd")){web.send(400,"text/plain","Missing cmd");return;}
        String cmd=web.arg("cmd");
        if(cmd.length()>96){web.send(400,"text/plain","Too long");return;}
        String result=executeCommand(cmd);
        web.send(result.startsWith("ERROR")||result.startsWith("REJECTED")?400:200,"text/plain",result);
    });
    web.begin();
}
void processWeb() {
    if (!strlen(WIFI_SSID) || !strcmp(WEB_PASS,"CHANGE_THIS_LONG_PASSWORD"))return;
    web.handleClient(); // fixed RX/TX byte budgets, fixed buffers, nonblocking sockets
    if(WiFi.status()!=WL_CONNECTED && millis()-lastWifiAttempt>30000UL) {
        lastWifiAttempt=millis();WiFi.reconnect();
    }
}

// Shared serial/web commands. Bench-only injection is compiled out of Main.
String executeCommand(String line) {
    line.trim(); line.toLowerCase();
    int space = line.indexOf(' ');
    String cmd = space < 0 ? line : line.substring(0,space);
    String value = space < 0 ? String("") : line.substring(space+1);
    char *numberEnd = nullptr;
    float f = strtof(value.c_str(), &numberEnd);
    bool validNumber = value.length() && numberEnd != value.c_str() && *numberEnd == '\0' && isfinite(f);
    if (cmd == "alloff" || (cmd == "service" && value == "on")) {
        return setServiceMode(true) ? "OK: SERVICE ON; all outputs OFF; persists across restart" : "ERROR: stopped; service persistence failed";
    }
    if (cmd == "service" && value == "off confirm") {
        return setServiceMode(false) ? "OK: SERVICE OFF; automatic control enabled" : "REJECTED: clear fault/settings failure first";
    }
    if (cmd == "service") return "ERROR use service on|off confirm";
    if (serviceMode && (cmd == "low" || cmd == "high" || cmd == "heat" || cmd == "ready" || cmd == "afteruse" || cmd == "light" || cmd == "lightprog"))
        return "REJECTED: Service Mode";
    // V0.9: 2 s lockout between jets (pump) and light start changes; stops are never blocked.
    bool pumpStart = cmd == "low" || cmd == "heat" || cmd == "high";
    bool lightStart = (cmd == "light" && (value == "on" || (value.length() == 0 && !lightOn))) || cmd == "lightprog";
    if ((pumpStart && spa.fault == FaultCode::NONE && inputLocked(true)) || (lightStart && inputLocked(false)))
        return "REJECTED: input lockout (2 s)";
    if (spa.fault != FaultCode::NONE && (cmd == "low" || cmd == "high" || cmd == "heat" || cmd == "ready" || cmd == "afteruse"))
        return "REJECTED: active fault";
    if (cmd == "low" || cmd == "heat") {noteInteraction(); manualPumpRequest = true; manualLowStartMs = millis(); noteInputChange(true); requestPumpMode(PumpMode::LOW_SPEED);}
    else if (cmd == "high") {noteInteraction(); manualPumpRequest = false; purgeActive = false; noteInputChange(true); requestPumpMode(PumpMode::HIGH_SPEED);}
    else if (cmd == "off") {noteInteraction(); manualPumpRequest = false; requestPumpMode(PumpMode::OFF); return "OK: manual pump OFF; automatic LOW/after-use may resume";}
    else if (cmd == "setpoint" && validNumber && f >= 20 && f <= USER_MAX_SETPOINT_C) {
        noteInteraction(); config.setpointC = f; if (!persistConfiguration()) return "ERROR: settings not saved; outputs stopped";
    }
    else if (cmd == "light") {
        if (value.length() && value != "on" && value != "off") return "ERROR use light [on|off]";
        bool want = value == "on" ? true : value == "off" ? false : !lightOn;
        noteInteraction(); if (want) noteInputChange(false); lightSetOn(want, millis());
    }
    else if (cmd == "lightprog") {
        if (!validNumber || f < 1 || f > LIGHT_PROGRAM_COUNT || f != (float)(int)f) return "ERROR use lightprog 1-" + String(LIGHT_PROGRAM_COUNT);
        noteInteraction(); noteInputChange(false); lightSelectProgram((uint8_t)f, millis());
    }
    else if (cmd == "primeend") {
        if (!primingActive) return "OK: not priming";
        primingActive = false; logEvent("PRIMING ENDED EARLY");
    }
    else if (cmd == "heatmode") { // solar-window vs manual enable vs heating disabled
        if (value == "off") heatMode = 0;
        else if (value == "solar") heatMode = 1;
        else if (value == "manual") heatMode = 2;
        else return "ERROR invalid heatmode";
        if (heatMode == 0) allHeaterOutputsOff();
        if (!persistConfiguration()) return "ERROR: settings not saved; outputs stopped";
    }
    else if (cmd == "window") {
        int sep = value.indexOf('-');
        int a = sep < 0 ? -1 : value.substring(0,sep).toInt();
        int b = sep < 0 ? -1 : value.substring(sep+1).toInt();
        if (a < 0 || a > 23 || b < 0 || b > 23) return "ERROR use window 9-16";
        String canonical = String(a) + "-" + String(b);
        if (value != canonical) return "ERROR use window 11-16";
        solarStart = a; solarEnd = b;
        if (!persistConfiguration()) return "ERROR: settings not saved; outputs stopped";
    }
    else if (cmd == "resetfault") {
        // Reset only after fresh valid measurements and high-limit clearance.
        readTemperatureSensors();
        if (((SPA_BENCH_DEMO || PIN_HIGH_LIMIT_OK >= 0) && !hardwareHighLimitOK()) || tempA.status != SensorStatus::OK ||
            tempB.status != SensorStatus::OK || !isfinite(tempA.filteredC) ||
            !isfinite(tempB.filteredC) ||
            !isfinite(tempA.rawC) || !isfinite(tempB.rawC) ||
            fmax(tempA.rawC,tempB.rawC) >= ABSOLUTE_TEMP_TRIP_C ||
            fmax(tempA.filteredC,tempB.filteredC) >= ABSOLUTE_TEMP_TRIP_C ||
            fabs(tempA.filteredC-tempB.filteredC) >= TEMP_DIFF_INHIBIT_C)
            return "REJECTED: unsafe inputs";
        if (!configurationValid || (!SPA_BENCH_DEMO && !adcAvailable))
            return "REJECTED: repair configuration/ADC and restart";
        manualPumpRequest = false; heatPumpNeeded = false;
        allHeaterOutputsOff(); spa.fault = FaultCode::NONE; spa.lockedOut = false;
        spa.heatQualificationPassed = false; spa.thermostatHeating = false;
        if (!persistSafety()) return "ERROR: fault acknowledgement not saved; outputs stopped";
        requestPumpMode(PumpMode::OFF); logEvent("FAULT RESET"); return "OK: fault reset; pump OFF";
    }
#if SPA_BENCH_DEMO
    else if (cmd == "prime") { primingActive = true; primingUntilMs = millis() + PRIMING_MS; logEvent("PRIMING (demo)"); }
    else if (cmd == "tempa" && validNumber) demoTempA = f;
    else if (cmd == "tempb" && validNumber) demoTempB = f;
    else if (cmd == "sensorafail") demoSensorAFailed = true;
    else if (cmd == "sensorbfail") demoSensorBFailed = true;
    else if (cmd == "highlimit") demoHighLimitOK = false;
    else if (cmd == "normal") {demoTempA=demoTempB=36; demoSensorAFailed=demoSensorBFailed=false; demoHighLimitOK=true;}
    else if (cmd == "diff") {demoTempA=36; demoTempB=40;}
    else if (cmd == "overtemp") {demoTempA=demoTempB=44;}
#endif
    else if (cmd == "ready") noteInteraction();
    else if (cmd == "standby") readyMode = false;
    else if (cmd == "afteruse") {noteInteraction(); afterUseRemainingMs = AFTER_USE_SEC * 1000UL; saveRuntime();}
    else if (cmd == "filtercleaned" && value == "confirm") {
        sinceCleanLowSec = 0;
        time_t stamp = time(nullptr);
        cleanEpoch = stamp > 1700000000 ? (uint32_t)stamp : 0;
        saveRuntime(); logEvent("FILTER CLEANED");
    }
    else if (cmd == "filterstatus") {
        return "LOW today (commanded) sec=" + String(dailyLowSec) +
               " / " + String(DAILY_FILTER_TARGET_SEC) +
               ", since clean sec=" + String(sinceCleanLowSec) +
               ", days=" + String(cleanAgeDays()) +
               ", after-use remaining sec=" + String(afterUseRemainingMs/1000UL) +
               ", clean due=" + String((sinceCleanLowSec >= FILTER_CLEAN_SEC ||
                    cleanAgeDays() >= FILTER_CLEAN_AGE_DAYS) ? "YES":"NO");
    }
#if SPA_BENCH_DEMO
    else if (cmd == "clock") {
        int colon = value.indexOf(':');
        int h = colon<0 ? -1 : value.substring(0,colon).toInt();
        int m = colon<0 ? -1 : value.substring(colon+1).toInt();
        if(h<0 || h>23 || m<0 || m>59) return "ERROR use clock HH:MM";
        if (value.length() != 5 || value[2] != ':' || !isdigit(value[0]) || !isdigit(value[1]) || !isdigit(value[3]) || !isdigit(value[4])) return "ERROR use clock HH:MM";
        demoMinuteOfDay = h*60+m; demoClockBaseMs=millis();
    }
#endif
    else if (cmd == "events") return eventsText();
    else if (cmd == "status") { printStatus(); return statusJSON(); }
    else if (cmd == "help") return "low high off alloff service on|off confirm heat setpoint N light [on|off] lightprog N primeend heatmode off|solar|manual window 11-16 ready standby afteruse filterstatus filtercleaned resetfault status events (demo: prime tempA N tempB N diff normal overtemp sensorafail sensorbfail highlimit)";
    else return "ERROR unknown command";
    if (!storageHealthy) return "ERROR: settings storage failure; outputs stopped";
    return "OK";
}
void processSerialCommands() {
    static String line;
    static bool overflow = false;
    unsigned budget = 128;
    while (budget-- && Serial.available()) {
        char c=Serial.read();
        if (c=='\r' || c=='\n') {
            if (overflow) Serial.println("ERROR too long");
            else if (line.length()) Serial.println(executeCommand(line));
            line=""; overflow=false;
        } else if (line.length()<96) line+=c;
        else overflow=true;
    }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    v09BootMs = millis();
    initialiseOutputs();
    Serial.begin(115200);

    delay(500);

    Serial.println();
    Serial.println("SPA CONTROLLER STARTING");

    initialiseOutputs();

    loadSafety();
    loadConfiguration();
    loadRuntime();
    resetReason = (int)esp_reset_reason();
    if (resetReason == ESP_RST_TASK_WDT || resetReason == ESP_RST_INT_WDT ||
        resetReason == ESP_RST_WDT || resetReason == ESP_RST_PANIC || resetReason == ESP_RST_BROWNOUT) {
        serviceMode = true;
        forceShutdown();
        raiseFault(resetReason == ESP_RST_BROWNOUT ? FaultCode::POWER_FAULT : FaultCode::WATCHDOG_RESET, true);
        persistSafety();
    }
    setupWatchdog();
    setupWeb();

#if SPA_TEMP_ADC_ADS1115
    // Explicit free header pins: generic S3 defaults can conflict with board IO.
    Wire.begin(8, 9); // SDA GPIO8, SCL GPIO9: wire the ADS1115 accordingly.
    Wire.setTimeOut(50);
#endif
    #if !SPA_BENCH_DEMO && !SPA_TEMP_ADC_ADS1115
    // V0.9 onboard ADC1: GPIO8/GPIO9 analog inputs, 11 dB attenuation (~0.15-3.1 V).
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    adcAvailable = true;
    #endif
    #if !SPA_BENCH_DEMO && SPA_TEMP_ADC_ADS1115
    if (!ads.begin())
    {
        Serial.println("ADS1115 FAILURE");

        raiseFault(
            FaultCode::ADC_FAILURE,
            true
        );

        return;
    }

    adcAvailable = true;
    ads.setGain(GAIN_ONE);
    #endif
    // +/-4.096V

    if (spa.fault == FaultCode::NONE) spa.state = SpaState::SELF_TEST;

    // Initial sensor reads.
    for (int i = 0; i < 5; ++i)
    {
        readTemperatureSensors();
        delay(200);
    }

    if (temperatureSystemHealthy() && spa.fault == FaultCode::NONE)
    {
        spa.state = serviceMode ? SpaState::SERVICE : SpaState::IDLE;
    }

    // V0.9: priming after a genuine power-on (water may have been drained).
    if (!SPA_BENCH_DEMO && resetReason == ESP_RST_POWERON && !serviceMode && spa.fault == FaultCode::NONE) {
        primingActive = true; primingUntilMs = millis() + PRIMING_MS;
        logEvent("PRIMING 4 MIN: heater locked out");
    }
#if SPA_BENCH_DEMO
    Serial.println("V0.9 DEMO: CH1/CH3 PHYSICALLY INHIBITED; OTHER RELAYS LIVE");
    Serial.println("Type help. NEVER CONNECT THIS BUILD TO MAINS OR SPA HEATER.");
#else
    Serial.println("V0.9 MAIN: HEATER DISABLED PENDING INDEPENDENT SAFETY COMMISSIONING");
#endif
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // Check old timestamps BEFORE acquisition can refresh them.
    checkSensorFreshness();
    processSerialCommands();
    processWeb();
    checkSensorFreshness();
    static uint32_t lastTempMs = 0;
    static uint32_t lastControlMs = 0;
    static uint32_t lastStatusMs = 0;

    uint32_t now = millis();

    // --------------------------------------------------------
    // Temperature acquisition
    // --------------------------------------------------------

    if (
        now - lastTempMs >=
        TEMP_SAMPLE_MS
    )
    {
        lastTempMs = now;

        readTemperatureSensors();

        temperatureSystemHealthy();
    }

    // --------------------------------------------------------
    // Main safety/control loop
    // --------------------------------------------------------

    if (
        now - lastControlMs >=
        CONTROL_LOOP_MS
    )
    {
        lastControlMs = now;

        if ((SPA_BENCH_DEMO || PIN_HIGH_LIMIT_OK >= 0) && !hardwareHighLimitOK())
            raiseFault(FaultCode::HIGH_LIMIT_OPEN, true);
        processCombinedCirculation();
        processPumpRequest();
        trackPumpRun(millis());
        processSensorAgreement(millis());

        processHeater();

        processUV();
        processLight(millis());

        updateSpaState();
    }

    // --------------------------------------------------------
    // Diagnostics
    // --------------------------------------------------------

    if (
        now - lastStatusMs >=
        STATUS_LOOP_MS
    )
    {
        lastStatusMs = now;

        printStatus();
    }

    // --------------------------------------------------------
    // FUTURE TASKS
    // --------------------------------------------------------

    // processML250();
    // Wi-Fi HTTP implemented above; HTTPS/TLS or MQTT not yet added.
    // processMQTT();
    // processMatter();
    // processContactorFeedback();
    // processHeaterCurrent();
    // processFilterSchedule();
    // processFreezeProtection();
    if (serviceMode) forceShutdown();
    // Feed only after a complete safety/control iteration, never inside IO waits.
    if (controlWatchdog && esp_task_wdt_reset_user(controlWatchdog) != ESP_OK) {
        serviceMode = true;
        forceShutdown();
        raiseFault(FaultCode::WATCHDOG_RESET, true);
        persistSafety();
    }
}