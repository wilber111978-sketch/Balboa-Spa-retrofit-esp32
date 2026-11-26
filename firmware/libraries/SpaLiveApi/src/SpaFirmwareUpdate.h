#pragma once

#include <ArduinoOTA.h>
#include <Update.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <esp_arduino_version.h>

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 12)
#error "V0.9.2 Wi-Fi updates require Arduino ESP32 core 3.3.12 or newer."
#endif

#if !defined(CONFIG_APP_ROLLBACK_ENABLE) || !CONFIG_APP_ROLLBACK_ENABLE
#error "V0.9.2 Wi-Fi updates require an ESP32 build with app rollback enabled."
#endif

// Delay Arduino's default automatic confirmation until the controller has run.
extern "C" bool verifyRollbackLater() { return true; }

namespace spaUpdate {

constexpr uint32_t listenWindowMs = 10UL * 60UL * 1000UL;
constexpr uint32_t verifyWindowMs = 60UL * 1000UL;
constexpr uint32_t healthyWindowMs = 10UL * 1000UL;
static bool pendingVerification = false;
// V0.9.3: the first live OTA test left the hold set after the 10 min window closed with no
// upload, which also blocked leaving Service Mode until a restart. The listener now drops the
// hold before tearing ArduinoOTA down, and the loop clears a hold left past the window.
static std::atomic<bool> listenerRunning{false};
static uint32_t armedAt = 0;
static bool staleLogged = false;
static uint32_t verificationStarted = 0;
static uint32_t healthySince = 0;

static void listen(void *)
{
    ArduinoOTA.setHostname("fisher-spa");
    ArduinoOTA.setPassword(SPA_OTA_PASSWORD);
    ArduinoOTA.onStart([] {
        if (ArduinoOTA.getCommand() != U_FLASH) Update.abort();
    });
    ArduinoOTA.begin();
    const uint32_t started = millis();
    while (millis() - started < listenWindowMs && WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (!Update.isRunning()) firmwareUpdateHold.store(false);
    ArduinoOTA.end();
    firmwareUpdateHold.store(false);
    listenerRunning.store(false);
    vTaskDelete(nullptr);
}

// V0.9.3: say which check refused, so a 409 is diagnosable from the dashboard.
static const char *refusal = "";
static bool refuse(const char *why) { refusal = why; return false; }
static bool arm()
{
    if (!serviceMode) return refuse("Service Mode is off");
    if (!storageHealthy) return refuse("settings storage unhealthy");
    if (spa.fault != FaultCode::NONE) return refuse("a fault is active");
    if (!controlWatchdog) return refuse("control watchdog not running");
    if (!liveInputsSafe()) return refuse("probe/high-limit inputs not healthy");
    if (WiFi.status() != WL_CONNECTED) return refuse("Wi-Fi not reported connected");
    if (pendingVerification) return refuse("previous update still awaiting its self-check (restart to retry it)");
    if (!esp_ota_get_next_update_partition(nullptr)) return refuse("no free OTA slot");
    if (listenerRunning.load()) return refuse("previous upload listener still running (restart first)");
    if (firmwareUpdateHold.exchange(true)) return refuse("an upload window is already open");
    listenerRunning.store(true);
    armedAt = millis();
    staleLogged = false;
    if (xTaskCreate(listen, "spa-ota", 12288, nullptr, 1, nullptr) != pdPASS) {
        listenerRunning.store(false);
        firmwareUpdateHold.store(false);
        return refuse("not enough free memory to start the upload listener");
    }
    return true;
}

static void beginVerification()
{
    esp_ota_img_states_t imageState;
    pendingVerification = esp_ota_get_state_partition(esp_ota_get_running_partition(), &imageState) == ESP_OK &&
                          imageState == ESP_OTA_IMG_PENDING_VERIFY;
    if (!pendingVerification) return;
    firmwareUpdateHold.store(true);
    verificationStarted = millis();
    // Service was persisted before the upload. A missing hold is a failed boot.
    if (!serviceMode || !storageHealthy) {
        forceShutdown();
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

static void clearStaleHold()
{
    if (pendingVerification || !firmwareUpdateHold.load() || Update.isRunning()) return;
    if (millis() - armedAt < listenWindowMs + 60000UL) return;
    firmwareUpdateHold.store(false);
    if (!staleLogged) {
        staleLogged = true;
        logEvent(listenerRunning.load()
            ? "OTA WINDOW OVERRAN: hold cleared; restart the controller before the next Wi-Fi upload"
            : "OTA WINDOW OVERRAN: hold cleared");
    }
}

static void verifyAfterLoop()
{
    clearStaleHold();
    if (!pendingVerification) return;
    const uint32_t now = millis();
    if (now - verificationStarted >= verifyWindowMs) {
        esp_ota_mark_app_invalid_rollback_and_reboot();
        return;
    }
    if (!serviceMode || !controlWatchdog || spa.fault != FaultCode::NONE ||
        !liveInputsSafe() || WiFi.status() != WL_CONNECTED) {
        healthySince = 0;
        return;
    }
    if (!healthySince) healthySince = now;
    if (now - healthySince < healthyWindowMs) return;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        pendingVerification = false;
        firmwareUpdateHold.store(false);
        logEvent("OTA VERIFIED: SERVICE REMAINS ON");
    }
}

} // namespace spaUpdate
