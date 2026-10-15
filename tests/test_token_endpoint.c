#include "token_endpoint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    token_endpoint_t e;
    assert(token_endpoint_parse_https("https://codex.example/v1/board",&e));
    assert(!strcmp(e.host,"codex.example") && !strcmp(e.path,"/v1/board") && e.port==443);
    assert(token_endpoint_parse_https("https://codex.example:8443/v1/board?q=1",&e));
    assert(e.port==8443 && !strcmp(e.path,"/v1/board?q=1"));
    assert(token_endpoint_parse_https("https://codex.example?x=1",&e) && !strcmp(e.path,"/?x=1"));
    assert(token_endpoint_parse_https("https://codex.example",&e) && !strcmp(e.path,"/"));
    const char *bad[]={"http://codex.example/","https:///x","https://user:pass@codex.example/",
        "https://codex.example:0/","https://codex.example:65536/","https://codex.example:abc/",
        "https://codex.example/\r\nHost:evil","https://codex.example/#fragment","https://codex.example@evil/"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) assert(!token_endpoint_parse_https(bad[i],&e));
    char long_url[512];snprintf(long_url,sizeof(long_url),"https://codex.example/%0300d",0);
    assert(!token_endpoint_parse_https(long_url,&e));
    uint32_t ips[]={1,2};assert(token_endpoint_pick(ips,2,2)==1);
    assert(token_endpoint_pick(ips,2,9)==0); /* DNS changed: obsolete preferred IP is not reused. */
    assert(token_endpoint_pick(ips,0,2)==0);
    puts("HTTPS endpoint authority, path preservation and DNS preference: PASS");
}
