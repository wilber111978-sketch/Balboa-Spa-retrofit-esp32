#pragma once
#define ESP_RST_POWERON 1
#define ESP_RST_PANIC 4
#define ESP_RST_INT_WDT 5
#define ESP_RST_TASK_WDT 6
#define ESP_RST_WDT 7
#define ESP_RST_BROWNOUT 9
inline int fakeResetReason=ESP_RST_POWERON;
inline int esp_reset_reason(){return fakeResetReason;}
