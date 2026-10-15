#include "token_ui_logic.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
void token_power_init(token_power_t *p) { memset(p, 0, sizeof(*p)); p->idle_ms=TOKEN_IDLE_MS; }
token_input_result_t token_power_input(token_power_t *p, bool confirm, uint64_t now) {
    if (!p->awake) {
        if (!confirm) return TOKEN_INPUT_IGNORE;
        p->awake=true; p->last_input_ms=now; p->wake_guard_ms=now+650;
        return TOKEN_INPUT_WAKE;
    }
    p->last_input_ms=now;
    if (confirm && now < p->wake_guard_ms) return TOKEN_INPUT_IGNORE;
    return TOKEN_INPUT_FORWARD;
}
bool token_power_tick(token_power_t *p, uint64_t now) {
    if (p->awake && p->idle_ms && now >= p->last_input_ms && now-p->last_input_ms >= p->idle_ms) {
        p->awake=false; return true;
    }
    return false;
}
void token_format_value(uint64_t n, char *number, size_t ns, char *unit, size_t us) {
    const uint64_t scales[]={1,1000,1000000,1000000000,UINT64_C(1000000000000),UINT64_C(1000000000000000),UINT64_C(1000000000000000000)};
    const char *units[]={"TOKENS","K TOKENS","M TOKENS","B TOKENS","T TOKENS","P TOKENS","E TOKENS"};
    unsigned i=0;
    while (i<6 && n>=scales[i+1]) i++;
    if (!i) snprintf(number,ns,"%" PRIu64,n);
    else {
        uint64_t whole=n/scales[i], tenth=(n%scales[i]*10+scales[i]/2)/scales[i];
        if (tenth==10) { whole++; tenth=0; }
        if (whole>=1000 && i<6) { i++; whole=1; tenth=0; }
        snprintf(number,ns,"%" PRIu64 ".%" PRIu64,whole,tenth);
    }
    snprintf(unit,us,"%s",units[i]);
}
uint8_t token_digit_mask(char digit) {
    const uint8_t masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    if (digit>='0' && digit<='9') return masks[digit-'0'];
    return digit=='-' ? 0x40 : 0;
}
int token_digit_advance(char digit) { return digit=='.' ? 15 : 44; }
token_rect_t token_segment_rect(unsigned i) {
    const token_rect_t rects[]={{5,0,20,6},{25,6,6,24},{25,36,6,24},{5,60,20,6},{0,36,6,24},{0,6,6,24},{5,30,20,6}};
    return i<7 ? rects[i] : (token_rect_t){0};
}

int token_filled_cells(int percent) { return percent<=0?0:percent>=100?10:percent/10; }

int token_navigation_next(int page, bool up) {
    if(page<0 || page>9) return 8;
    int step=up?9:1, next=(page+step)%10;
    if(next==7) next=(next+step)%10;
    return next;
}

static const uint32_t brightness_levels[]={10,20,35,50,70,100};
bool token_brightness_valid(uint32_t percent) {
    for(unsigned i=0;i<sizeof(brightness_levels)/sizeof(brightness_levels[0]);i++)
        if(percent==brightness_levels[i]) return true;
    return false;
}
uint32_t token_brightness_next(uint32_t percent) {
    unsigned count=sizeof(brightness_levels)/sizeof(brightness_levels[0]);
    for(unsigned i=0;i<count;i++) if(percent==brightness_levels[i]) return brightness_levels[(i+1)%count];
    return TOKEN_BRIGHTNESS_DEFAULT;
}

uint32_t token_sync_delay(uint32_t minutes, unsigned failures) {
    if(!minutes) return 0; /* Manual mode disables scheduled retries too. */
    if(failures) {
        const uint32_t delays[]={30,60,120,300};
        return delays[failures>=4?3:failures-1];
    }
    return minutes>UINT32_MAX/60?UINT32_MAX:minutes*60;
}

unsigned token_sync_failure_count(unsigned previous,bool received,bool stale) {
    if(received && !stale) return 0;
    return previous>=4?4:previous+1;
}

static const uint32_t sleep_levels[]={15,30,60,120,300,0};
bool token_sleep_valid(uint32_t seconds) {
    for(unsigned i=0;i<sizeof(sleep_levels)/sizeof(sleep_levels[0]);i++)
        if(seconds==sleep_levels[i]) return true;
    return false;
}
uint32_t token_sleep_next(uint32_t seconds) {
    unsigned count=sizeof(sleep_levels)/sizeof(sleep_levels[0]);
    for(unsigned i=0;i<count;i++) if(seconds==sleep_levels[i]) return sleep_levels[(i+1)%count];
    return TOKEN_IDLE_MS/1000;
}
void token_power_set_sleep(token_power_t *p,uint32_t seconds) {
    p->idle_ms=(token_sleep_valid(seconds)?seconds:TOKEN_IDLE_MS/1000)*1000;
}
uint32_t token_sync_pending_after_fetch(uint32_t requests) {
    return requests & ~TOKEN_NET_REFRESH;
}
