#include "token_updates.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct test_queue { size_t size;bool occupied;unsigned char value[]; };
static int creates,fail_on;
QueueHandle_t xQueueCreate(UBaseType_t length,UBaseType_t size) {
    assert(length==1);
    if(++creates==fail_on) return NULL;
    QueueHandle_t q=calloc(1,sizeof(*q)+size);assert(q);q->size=size;return q;
}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *value) {
    memcpy(q->value,value,q->size);q->occupied=true;return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *value,TickType_t wait) {
    (void)wait;if(!q->occupied) return pdFALSE;
    memcpy(value,q->value,q->size);q->occupied=false;return pdTRUE;
}
void vQueueDelete(QueueHandle_t q) {free(q);}
int main(void) {
    token_updates_t updates;token_board_t in={.fetched=123},out;int status=-1;
    assert(token_updates_init(&updates));
    token_updates_publish(&updates,0,&in);
    token_updates_publish(&updates,1,NULL); /* Immediate next request used to overwrite this result. */
    token_updates_publish(&updates,7,NULL); /* Later TLS error must also preserve the received data. */
    assert(token_updates_take_board(&updates,&out) && out.fetched==123);
    assert(!token_updates_take_board(&updates,&out));
    assert(token_updates_take_status(&updates,&status) && status==7);
    assert(!token_updates_take_status(&updates,&status));
    in.fetched=456;token_updates_publish(&updates,0,&in);
    in.fetched=789;token_updates_publish(&updates,0,&in);
    assert(token_updates_take_board(&updates,&out) && out.fetched==789);
    assert(token_updates_take_status(&updates,&status) && status==0);
    vQueueDelete(updates.board);vQueueDelete(updates.status);
    creates=0;fail_on=2;
    assert(!token_updates_init(&updates) && !updates.board && !updates.status);
    puts("Pending board survives progress/error updates; latest board and allocation rollback: PASS");
}
