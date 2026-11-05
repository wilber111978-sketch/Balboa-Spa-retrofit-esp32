#pragma once
// Single NVS blob per logical record. CRC detects corruption; NVS supplies the
// atomic blob commit. A presence bit prevents a missing new-format record from
// silently falling back to stale legacy keys. CRC is not authentication.
struct ConfigRecord {
    uint32_t version;
    SpaConfig values;
    uint32_t mode, start, end, crc;
};
struct SafetyRecord { uint32_t version, service, fault, crc; };
struct RuntimeRecord { uint32_t version, afteruse, sinceclean, cleanepoch, day, seconds, crc; };
static constexpr uint32_t SETTINGS_VERSION = 2;
enum class RecordResult { ABSENT, VALID, INVALID };

uint32_t recordCRC(const void *data, size_t n) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}
template<class T> RecordResult readRecord(const char *key, uint32_t presence, T &record) {
    if (!prefs.begin("spa", false)) return RecordResult::INVALID;
    bool present = prefs.isKey(key);
    uint32_t format = prefs.getUInt("format", 0);
    bool ok = present && prefs.getBytesLength(key) == sizeof(T) &&
        prefs.getBytes(key, &record, sizeof(T)) == sizeof(T);
    prefs.end();
    if (!present) return format & presence ? RecordResult::INVALID : RecordResult::ABSENT;
    return ok && record.version == SETTINGS_VERSION &&
        record.crc == recordCRC(&record, offsetof(T, crc)) ? RecordResult::VALID : RecordResult::INVALID;
}
template<class T> bool writeRecord(const char *key, uint32_t presence, T &record) {
    record.version = SETTINGS_VERSION;
    record.crc = recordCRC(&record, offsetof(T, crc));
    if (!prefs.begin("spa", false)) { storageFailure(); return false; }
    T verify{};
    bool ok = prefs.putBytes(key, &record, sizeof(T)) == sizeof(T) &&
        prefs.getBytesLength(key) == sizeof(T) && prefs.getBytes(key, &verify, sizeof(T)) == sizeof(T) &&
        memcmp(&record, &verify, sizeof(T)) == 0;
    uint32_t format = prefs.getUInt("format", 0) | presence;
    if (ok) ok = prefs.putUInt("format", format) == sizeof(uint32_t) && prefs.getUInt("format", 0) == format;
    prefs.end();
    if (!ok) storageFailure();
    return ok;
}
bool configValuesValid(const SpaConfig &c, uint32_t mode, uint32_t start, uint32_t end) {
    return isfinite(c.setpointC) && c.setpointC >= 20 && c.setpointC <= USER_MAX_SETPOINT_C &&
        isfinite(c.sensorAR25) && c.sensorAR25 >= 1000 && c.sensorAR25 <= 100000 &&
        isfinite(c.sensorBR25) && c.sensorBR25 >= 1000 && c.sensorBR25 <= 100000 &&
        isfinite(c.sensorABeta) && c.sensorABeta >= 1000 && c.sensorABeta <= 6000 &&
        isfinite(c.sensorBBeta) && c.sensorBBeta >= 1000 && c.sensorBBeta <= 6000 &&
        isfinite(c.sensorAOffset) && fabs(c.sensorAOffset) <= 5 &&
        isfinite(c.sensorBOffset) && fabs(c.sensorBOffset) <= 5 && mode <= 2 && start <= 23 && end <= 23;
}
bool persistConfiguration() {
    if (!storageHealthy || !configValuesValid(config, heatMode, solarStart, solarEnd)) { storageFailure(); return false; }
    ConfigRecord record{};
    record.values = config; record.mode = heatMode; record.start = solarStart; record.end = solarEnd;
    return writeRecord("config2", 1, record);
}
void restoreConfiguration() {
    ConfigRecord record{};
    RecordResult result = readRecord("config2", 1, record);
    if (result == RecordResult::ABSENT) {
        if (!prefs.begin("spa", false)) { storageFailure(); return; }
        record.values.setpointC = prefs.getFloat("setpoint",38);
        record.values.sensorAR25 = prefs.getFloat("a_r25",DEFAULT_R25);
        record.values.sensorABeta = prefs.getFloat("a_beta",DEFAULT_BETA);
        record.values.sensorAOffset = prefs.getFloat("a_off",0);
        record.values.sensorBR25 = prefs.getFloat("b_r25",DEFAULT_R25);
        record.values.sensorBBeta = prefs.getFloat("b_beta",DEFAULT_BETA);
        record.values.sensorBOffset = prefs.getFloat("b_off",0);
        record.mode = prefs.getUChar("heatmode",0);
        record.start = prefs.getUChar("start",11); record.end = prefs.getUChar("end",16);
        prefs.end();
    }
    if (result == RecordResult::INVALID || !configValuesValid(record.values,record.mode,record.start,record.end)) {
        config = SpaConfig{}; heatMode = 0; solarStart = 11; solarEnd = 16;
        storageFailure(); return;
    }
    config = record.values; heatMode = record.mode; solarStart = record.start; solarEnd = record.end;
    configurationValid = storageHealthy;
    if (result == RecordResult::ABSENT && !persistConfiguration()) return;
    if (heatMode == 2) heatMode = 0; // unchanged restart policy
}
bool persistSafety() {
    if (!storageHealthy) return false;
    SafetyRecord record{};
    record.service = serviceMode; record.fault = (uint32_t)spa.fault;
    return writeRecord("safety2", 2, record);
}
void loadSafety() {
    SafetyRecord record{};
    RecordResult result = readRecord("safety2", 2, record);
    if (result == RecordResult::ABSENT) {
        if (!prefs.begin("spa", false)) { storageFailure(); return; }
        record.fault = prefs.getUInt("fault",0); prefs.end();
    }
    if (result == RecordResult::INVALID || record.service > 1 || record.fault > (uint32_t)FaultCode::SENSOR_STALE) {
        storageFailure(); return;
    }
    serviceMode = record.service;
    if (record.fault) { spa.fault = (FaultCode)record.fault; spa.lockedOut = true; }
    if (result == RecordResult::ABSENT) persistSafety();
}
bool persistRuntime() {
    if (!storageHealthy) return false;
    RuntimeRecord record{};
    record.afteruse = afterUseRemainingMs; record.sinceclean = sinceCleanLowSec;
    record.cleanepoch = cleanEpoch; record.day = (uint32_t)lastDayKey; record.seconds = dailyLowSec;
    return writeRecord("runtime2", 4, record);
}
void restoreRuntime() {
    RuntimeRecord record{};
    RecordResult result = readRecord("runtime2", 4, record);
    if (result == RecordResult::ABSENT) {
        if (!prefs.begin("spa", false)) { storageFailure(); return; }
        record.afteruse = prefs.getUInt("afteruse",0); record.sinceclean = prefs.getUInt("sinceclean",0);
        record.cleanepoch = prefs.getUInt("cleanepoch",0); record.day = prefs.getUInt("filterday",UINT32_MAX);
        record.seconds = prefs.getUInt("filtersecs",0); prefs.end();
    }
    if (result == RecordResult::INVALID || record.afteruse > AFTER_USE_SEC * 1000UL || record.seconds > 86400UL) {
        storageFailure(); return;
    }
    afterUseRemainingMs = record.afteruse; sinceCleanLowSec = record.sinceclean;
    cleanEpoch = record.cleanepoch; lastDayKey = (int32_t)record.day; dailyLowSec = record.seconds;
    if (result == RecordResult::ABSENT) persistRuntime();
}
