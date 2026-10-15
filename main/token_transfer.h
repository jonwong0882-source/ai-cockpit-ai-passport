#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TOKEN_CONNECT_TIMEOUT_MS 10000
#define TOKEN_READ_TIMEOUT_MS 8000
#define TOKEN_ATTEMPT_MS 20000
#define TOKEN_REUSE_MS 45000
/* Small replies are collected without heap allocation; oversized replies fail. */
typedef struct {
    char body[6144];
    size_t used;
    bool overflow;
    int64_t started_ms, connected_ms, sent_ms, first_ms;
} token_transfer_t;
void token_transfer_start(token_transfer_t *t,int64_t now);
void token_transfer_data(token_transfer_t *t,const char *data,size_t length);
int token_transfer_timeout(const token_transfer_t *t,int64_t now);
bool token_transfer_reusable(int64_t last,int64_t now,bool same_address);
