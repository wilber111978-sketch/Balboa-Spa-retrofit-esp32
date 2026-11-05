#pragma once
#include "Arduino.h"
using esp_err_t=int;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 259
struct esp_task_wdt_config_t{uint32_t timeout_ms,idle_core_mask;bool trigger_panic;};
using esp_task_wdt_user_handle_t=void*;
inline esp_task_wdt_config_t fakeWdtConfig{};
inline int fakeWdtInitResult=0,fakeWdtFeedResult=0,fakeWdtFeeds=0,fakeWdtReconfigs=0;
inline uint32_t fakeWdtLastFeed=0;
inline esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t*c){fakeWdtConfig=*c;return fakeWdtInitResult;}
inline esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t*c){fakeWdtConfig=*c;++fakeWdtReconfigs;return 0;}
inline esp_err_t esp_task_wdt_add_user(const char*,void**h){*h=(void*)1;fakeWdtLastFeed=millis();return 0;}
inline esp_err_t esp_task_wdt_reset_user(void*){++fakeWdtFeeds;fakeWdtLastFeed=millis();return fakeWdtFeedResult;}
