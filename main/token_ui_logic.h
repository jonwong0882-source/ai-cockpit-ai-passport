#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TOKEN_IDLE_MS 30000
typedef struct { bool awake; uint64_t last_input_ms, wake_guard_ms; uint32_t idle_ms; } token_power_t;
typedef enum { TOKEN_INPUT_IGNORE, TOKEN_INPUT_WAKE, TOKEN_INPUT_FORWARD } token_input_result_t;
void token_power_init(token_power_t *p);
token_input_result_t token_power_input(token_power_t *p, bool confirm, uint64_t now_ms);
bool token_power_tick(token_power_t *p, uint64_t now_ms);
void token_format_value(uint64_t value, char *number, size_t number_size, char *unit, size_t unit_size);
uint8_t token_digit_mask(char digit);
int token_digit_advance(char digit);
typedef struct { int x, y, w, h; } token_rect_t;
token_rect_t token_segment_rect(unsigned segment);

int token_filled_cells(int percent);

int token_navigation_next(int page, bool up);

#define TOKEN_BRIGHTNESS_DEFAULT 35
bool token_brightness_valid(uint32_t percent);
uint32_t token_brightness_next(uint32_t percent);

/* Fetch outcomes share the UI queue; 1..6 remain connection/provisioning states. */
enum {
    TOKEN_FETCH_OK=0, TOKEN_FETCH_TLS=7, TOKEN_FETCH_DNS,
    TOKEN_FETCH_HTTP, TOKEN_FETCH_AUTH, TOKEN_FETCH_BODY,
    TOKEN_FETCH_FORMAT, TOKEN_FETCH_MEMORY, TOKEN_FETCH_CONFIG
};
uint32_t token_sync_delay(uint32_t interval_minutes, unsigned failures);

unsigned token_sync_failure_count(unsigned previous,bool received,bool stale);

bool token_sleep_valid(uint32_t seconds);
uint32_t token_sleep_next(uint32_t seconds);
void token_power_set_sleep(token_power_t *p,uint32_t seconds);
#define TOKEN_NET_REFRESH 1u
#define TOKEN_NET_RECONNECT 2u
#define TOKEN_NET_RESCHEDULE 4u
uint32_t token_sync_pending_after_fetch(uint32_t requests);
