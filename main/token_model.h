#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint64_t tokens;
    int reported_days, days;
    bool available, quota_available;
    int quota_percent, quota_days;
    char start[11], end[11];
} token_period_t;
#define TOKEN_HISTORY_DAYS 50
typedef struct {
    uint64_t daily_tokens[TOKEN_HISTORY_DAYS];
    bool daily_available[TOKEN_HISTORY_DAYS];
    char history_start[11], history_end[11];
    bool history_available;
    int completed_week_count;
    int completed_used[5];
    int64_t completed_reset[5];
    token_period_t periods[5];
    uint64_t lifetime;
    int streak;
    int used;
    int64_t reset, now, fetched;
    bool quota_available, lifetime_available, streak_available, stale;
    char latest[11];
} token_board_t;
bool token_board_parse(const char *json, token_board_t *output);
int token_progress(uint64_t tokens, uint64_t target);

int token_heat_level(uint64_t tokens, uint64_t maximum, bool available);
