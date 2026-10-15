#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { char host[128], path[256]; uint16_t port; } token_endpoint_t;
bool token_endpoint_parse_https(const char *url,token_endpoint_t *out);
unsigned token_endpoint_pick(const uint32_t *addresses,unsigned count,uint32_t preferred);
