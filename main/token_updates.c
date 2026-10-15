#include "token_updates.h"
#include <string.h>
bool token_updates_init(token_updates_t *updates) {
    memset(updates,0,sizeof(*updates));
    updates->board=xQueueCreate(1,sizeof(token_board_t));
    if(!updates->board) return false;
    updates->status=xQueueCreate(1,sizeof(int));
    if(!updates->status) {
        vQueueDelete(updates->board);
        updates->board=NULL;
        return false;
    }
    return true;
}
void token_updates_publish(token_updates_t *updates,int status,const token_board_t *board) {
    if(status==0) {
        if(!board) return;
        xQueueOverwrite(updates->board,board);
    }
    /* Progress or later errors must never discard a pending successful board. */
    xQueueOverwrite(updates->status,&status);
}
bool token_updates_take_board(token_updates_t *updates,token_board_t *board) {
    return xQueueReceive(updates->board,board,0)==pdTRUE;
}
bool token_updates_take_status(token_updates_t *updates,int *status) {
    return xQueueReceive(updates->status,status,0)==pdTRUE;
}
