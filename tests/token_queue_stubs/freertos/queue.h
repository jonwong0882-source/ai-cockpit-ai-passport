#pragma once
#include "FreeRTOS.h"
typedef struct test_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t length,UBaseType_t size);
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void *value);
BaseType_t xQueueReceive(QueueHandle_t queue,void *value,TickType_t wait);
void vQueueDelete(QueueHandle_t queue);
