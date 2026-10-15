#include "token_endpoint.h"
#include <string.h>
#include <stdio.h>
bool token_endpoint_parse_https(const char *url,token_endpoint_t *out) {
    if(!url || !out || strncmp(url,"https://",8)) return false;
    for(const unsigned char *c=(const unsigned char *)url;*c;c++) if(*c<=32 || *c>=127 || *c=='#') return false;
    token_endpoint_t e={.port=443};const char *p=url+8,*start=p;
    while((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='.' || *p=='-') p++;
    size_t n=(size_t)(p-start);if(!n || n>=sizeof(e.host)) return false;
    memcpy(e.host,start,n);
    if(*p==':') {
        unsigned port=0;p++;if(*p<'0' || *p>'9') return false;
        while(*p>='0' && *p<='9') {port=port*10+(unsigned)(*p++-'0');if(port>65535) return false;}
        if(!port) return false;
        e.port=(uint16_t)port;
    }
    int written;
    if(!*p) written=snprintf(e.path,sizeof(e.path),"/");
    else if(*p=='/') written=snprintf(e.path,sizeof(e.path),"%s",p);
    else if(*p=='?') written=snprintf(e.path,sizeof(e.path),"/%s",p);
    else return false;
    if(written<0 || (size_t)written>=sizeof(e.path)) return false;
    *out=e;return true;
}
unsigned token_endpoint_pick(const uint32_t *addresses,unsigned count,uint32_t preferred) {
    for(unsigned i=0;i<count;i++) if(addresses[i]==preferred) return i;
    return 0;
}
