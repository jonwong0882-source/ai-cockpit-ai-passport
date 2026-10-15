#include "token_ui_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(token_brightness_valid(TOKEN_BRIGHTNESS_DEFAULT));
    assert(!token_brightness_valid(0) && !token_brightness_valid(101));
    assert(token_brightness_next(100)==10 && token_brightness_next(999)==35);
    uint32_t level=35;unsigned levels=0;
    do {assert(token_brightness_valid(level));level=token_brightness_next(level);levels++;} while(level!=35 && levels<7);
    assert(levels==6 && level==35);
    assert(token_sync_delay(30,0)==1800);
    assert(token_sync_delay(30,1)==30 && token_sync_delay(30,2)==60);
    assert(token_sync_delay(30,3)==120 && token_sync_delay(30,4)==300);
    assert(token_sync_delay(30,1000)==300);
    assert(token_sync_delay(30,0)==1800); /* A successful refresh restores cadence. */
    assert(token_sync_delay(0,0)==0 && token_sync_delay(0,5)==0);
    assert(token_sync_delay(UINT32_MAX,0)==UINT32_MAX);
    unsigned failures=token_sync_failure_count(0,true,true);
    assert(failures==1 && token_sync_delay(30,failures)==30); /* Valid but stale API data needs a fresh-data retry. */
    failures=token_sync_failure_count(failures,false,false);
    assert(failures==2 && token_sync_delay(30,failures)==60);
    failures=token_sync_failure_count(failures,true,false);
    assert(failures==0 && token_sync_delay(30,failures)==1800);
    assert(token_sync_failure_count(99,false,false)==4);
    token_power_t p; token_power_init(&p);
    assert(!p.awake);
    assert(token_power_input(&p,false,10)==TOKEN_INPUT_IGNORE);
    assert(token_power_input(&p,true,100)==TOKEN_INPUT_WAKE);
    assert(token_power_input(&p,true,300)==TOKEN_INPUT_IGNORE); /* wake release */
    assert(token_power_input(&p,true,600)==TOKEN_INPUT_IGNORE); /* wake long/double */
    assert(token_power_input(&p,false,700)==TOKEN_INPUT_FORWARD);
    assert(token_power_input(&p,true,800)==TOKEN_INPUT_FORWARD);
    assert(!token_power_tick(&p,30799));
    assert(token_power_tick(&p,30800));
    assert(!token_power_tick(&p,90000));
    assert(token_power_input(&p,false,90001)==TOKEN_INPUT_IGNORE);
    assert(token_power_input(&p,true,90002)==TOKEN_INPUT_WAKE);
    assert(token_sleep_valid(0) && token_sleep_valid(30) && !token_sleep_valid(1));
    uint32_t sleep=30; unsigned sleep_count=0;
    do {assert(token_sleep_valid(sleep));sleep=token_sleep_next(sleep);sleep_count++;} while(sleep!=30 && sleep_count<7);
    assert(sleep_count==6 && token_sleep_next(999)==30);
    token_power_set_sleep(&p,15);
    assert(!token_power_tick(&p,105001) && token_power_tick(&p,105002));
    assert(token_power_input(&p,true,110000)==TOKEN_INPUT_WAKE);
    token_power_set_sleep(&p,0);
    assert(!token_power_tick(&p,UINT64_C(1000000000)) && p.awake);
    token_power_set_sleep(&p,300);
    assert(!token_power_tick(&p,409999) && token_power_tick(&p,410000));
    token_power_set_sleep(&p,999);
    assert(p.idle_ms==TOKEN_IDLE_MS);
    assert(token_sync_pending_after_fetch(TOKEN_NET_REFRESH)==0);
    assert(token_sync_pending_after_fetch(TOKEN_NET_REFRESH|TOKEN_NET_RECONNECT|TOKEN_NET_RESCHEDULE)==
           (TOKEN_NET_RECONNECT|TOKEN_NET_RESCHEDULE));
    char number[16],unit[16];
    token_format_value(0,number,sizeof(number),unit,sizeof(unit));assert(!strcmp(number,"0"));
    token_format_value(236383367,number,sizeof(number),unit,sizeof(unit));assert(!strcmp(number,"236.4") && !strcmp(unit,"M TOKENS"));
    token_format_value(999950,number,sizeof(number),unit,sizeof(unit));assert(!strcmp(number,"1.0") && !strcmp(unit,"M TOKENS"));
    token_format_value(UINT64_MAX,number,sizeof(number),unit,sizeof(unit));assert(!strcmp(number,"18.4") && !strcmp(unit,"E TOKENS"));
    for (unsigned n=0;n<10;n++) assert(token_digit_mask('0'+n));
    assert(token_digit_mask('?')==0 && token_digit_mask('-')==0x40);
    for (unsigned n=0;n<7;n++) {
        token_rect_t r=token_segment_rect(n);assert(r.x>=0 && r.y>=0 && r.x+r.w<=31 && r.y+r.h<=66);
    }
    assert(token_digit_advance('1')==44);
    assert(4*token_digit_advance('8')+token_digit_advance('.')<=220);
    assert(token_filled_cells(0)==0 && token_filled_cells(9)==0);
    assert(token_filled_cells(10)==1 && token_filled_cells(62)==6);
    assert(token_filled_cells(99)==9 && token_filled_cells(100)==10);
    assert(token_filled_cells(210)==10 && token_filled_cells(-1)==0);
    assert(token_navigation_next(6,false)==8 && token_navigation_next(8,true)==6);
    assert(token_navigation_next(9,false)==0 && token_navigation_next(0,true)==9);
    for(int start=0;start<10;start++) if(start!=7) {
        int page=start;unsigned visited=0;
        for(int i=0;i<9;i++) {assert(page!=7);assert(!(visited&(1u<<page)));visited|=1u<<page;int next=token_navigation_next(page,false);assert(token_navigation_next(next,true)==page);page=next;}
        assert(page==start && visited==0x37f);
    }
    puts("Token UI: wake gesture, idle deadline, numeric rollover and geometry PASS");
}
