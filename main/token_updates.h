#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "token_model.h"
typedef struct { QueueHandle_t board, status; } token_updates_t;
bool token_updates_init(token_updates_t *updates);
void token_updates_publish(token_updates_t *updates,int status,const token_board_t *board);
bool token_updates_take_board(token_updates_t *updates,token_board_t *board);
bool token_updates_take_status(token_updates_t *updates,int *status);
