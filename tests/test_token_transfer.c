#include "token_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    token_transfer_t t;token_transfer_start(&t,1000);
    token_transfer_data(&t,"{",1);token_transfer_data(&t,"}",1);
    assert(t.used==2 && !strcmp(t.body,"{}") && !t.overflow);
    assert(token_transfer_timeout(&t,1000)==8000);
    assert(token_transfer_timeout(&t,20000)==1000);
    assert(token_transfer_timeout(&t,22000)==1);
    char chunk[6144]={0};
    token_transfer_data(&t,chunk,sizeof(chunk));
    assert(t.overflow && t.used==2 && token_transfer_timeout(&t,1001)==1);
    token_transfer_start(&t,5000);assert(!t.overflow && !t.used && !t.body[0]);
    token_transfer_data(&t,chunk,sizeof(chunk)-1);assert(!t.overflow && t.used==6143);
    token_transfer_data(&t,"x",1);assert(t.overflow && t.used==6143);
    assert(token_transfer_reusable(1000,45999,true));
    assert(!token_transfer_reusable(1000,46000,true));
    assert(!token_transfer_reusable(1000,1001,false));
    assert(!token_transfer_reusable(1000,999,true));
    puts("HTTP transfer collection, overflow, budget and reuse expiry: PASS");
}
