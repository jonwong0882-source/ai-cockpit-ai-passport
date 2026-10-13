#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t token_wifi_start(void);
bool token_wifi_ready(void);
void token_wifi_pair(bool enable);
void token_wifi_retry(void);
void token_wifi_clear(void);
void token_wifi_poll(void);
bool token_wifi_https_allowed(void);
void token_wifi_https_busy(bool busy);
esp_err_t token_wifi_reconnect(void);
