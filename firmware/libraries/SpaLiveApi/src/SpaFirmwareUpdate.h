#pragma once

#include <ArduinoOTA.h>
#include <Update.h>
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
    ArduinoOTA.end();
    firmwareUpdateHold.store(false);
    vTaskDelete(nullptr);
}

static bool arm()
{
    if (!serviceMode || !storageHealthy || spa.fault != FaultCode::NONE ||
        !controlWatchdog || !liveInputsSafe() || WiFi.status() != WL_CONNECTED ||
        pendingVerification || !esp_ota_get_next_update_partition(nullptr)) {
        return false;
    }
    if (firmwareUpdateHold.exchange(true)) return false;
    if (xTaskCreate(listen, "spa-ota", 12288, nullptr, 1, nullptr) != pdPASS) {
        firmwareUpdateHold.store(false);
        return false;
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

static void verifyAfterLoop()
{
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
