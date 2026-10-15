#include "token_transfer.h"
#include <string.h>
void token_transfer_start(token_transfer_t *t,int64_t now) {
    memset(t,0,sizeof(*t));t->started_ms=now;
}
void token_transfer_data(token_transfer_t *t,const char *data,size_t length) {
    if(length>sizeof(t->body)-1-t->used) {t->overflow=true;return;}
    if(t->overflow) return;
    memcpy(t->body+t->used,data,length);t->used+=length;t->body[t->used]=0;
}
int token_transfer_timeout(const token_transfer_t *t,int64_t now) {
    int64_t left=TOKEN_ATTEMPT_MS-(now-t->started_ms);
    if(left<=0 || t->overflow) return 1;
    return left<TOKEN_READ_TIMEOUT_MS?(int)left:TOKEN_READ_TIMEOUT_MS;
}
bool token_transfer_reusable(int64_t last,int64_t now,bool same_address) {
    return same_address && now>=last && now-last<TOKEN_REUSE_MS;
}
