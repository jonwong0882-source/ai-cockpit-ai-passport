#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TOKEN_IDLE_MS 30000
typedef struct { bool awake; uint64_t last_input_ms, wake_guard_ms; } token_power_t;
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
