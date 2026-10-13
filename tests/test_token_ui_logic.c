#include "token_ui_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
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
