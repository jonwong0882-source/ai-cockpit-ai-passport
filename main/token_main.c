#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "token_wifi.h"
#include "token_model.h"
#include "token_ui_logic.h"
#include "lvgl.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
/* Community builds never include personal compile-time credentials. */
static char api_url[256];
static char api_key[81];
LV_FONT_DECLARE(token_digits_active);
LV_FONT_DECLARE(token_digits_dim);
static const char *TAG = "token_board";
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } token_key_event_t;
typedef struct { token_board_t board; int status; } update_t;
static QueueHandle_t keys, updates;
static TaskHandle_t net_task;
static token_board_t data;
static bool have_data, editing, pairing, cached;
static int page=8, battery = -1, network_status;
static int64_t received_us, pair_deadline;
static uint64_t targets[5], edit_target;
static token_power_t screen_power;
static int settings_selection, return_page;
enum { NET_REFRESH=1, NET_RECONNECT=2 };
static volatile uint32_t interval = 15;
static const uint32_t intervals[] = {5, 15, 30, 60, 0};
#define GREEN 0x00FFD0
#define DIM_GREEN 0x259D8E
#define INACTIVE_GREEN 0x103831
static lv_obj_t *title, *number, *detail, *goal, *footer, *power, *bar, *unit_label, *updated_at, *settings_rows[3];
static lv_obj_t *heatmap, *overview_stats, *overview_dates, *overview_legend, *middle_line;
static lv_obj_t *completion_panel, *period_dates, *period_coverage, *period_latest;
static char completion_text[5][32];
static int overview_range=2;
static const int overview_days[]={7,30,50};
static char digits[16]="--";
static int bar_percent;
static const char *names[]={"LAST 7D","YESTERDAY","LAST WEEK","LAST MONTH","THIS MONTH","WEEK QUOTA","AI EVERY DAY","SETTINGS","USAGE MAP","WEEK SCORE"};
static void number_text(char *out,size_t size,uint64_t n) {
    char u[16];token_format_value(n,out,size,u,sizeof(u));
    if (strcmp(u,"TOKENS")) { size_t used=strlen(out);if(used+1<size){out[used]=u[0];out[used+1]=0;} }
}
static void rect(lv_layer_t *layer,int x,int y,int w,int h,uint32_t color) {
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(color);d.bg_opa=LV_OPA_COVER;d.radius=0;
    lv_area_t a={.x1=x,.y1=y,.x2=x+w-1,.y2=y+h-1};lv_draw_rect(layer,&d,&a);
}
static void digit_draw(lv_event_t *e) {
    lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *layer=lv_event_get_layer(e);lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);
    d.text=digits;d.align=LV_TEXT_ALIGN_CENTER;d.font=&token_digits_dim;d.color=lv_color_hex(INACTIVE_GREEN);
    lv_draw_label(layer,&d,&a);d.font=&token_digits_active;d.color=lv_color_hex(GREEN);lv_draw_label(layer,&d,&a);
}
static void progress_draw(lv_event_t *e) {
    lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    for(int i=0;i<10;i++) rect(layer,a.x1+i*22,a.y1,19,9,
        i<token_filled_cells(bar_percent) ? GREEN : INACTIVE_GREEN);
}
static void completion_bar_draw(lv_event_t *e) {
    lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    for(int row=0;row<5;row++) {
        bool known=have_data && row<data.completed_week_count;
        lv_draw_label_dsc_t text;lv_draw_label_dsc_init(&text);
        text.font=&lv_font_unscii_8;text.color=lv_color_hex(known?GREEN:DIM_GREEN);
        text.text=completion_text[row];text.text_local=1;
        lv_area_t area={a.x1,a.y1+3+row*18,a.x1+99,a.y1+14+row*18};
        lv_draw_label(layer,&text,&area);
        for(int i=0;i<10;i++) rect(layer,a.x1+120+i*10,a.y1+4+row*18,8,9,
            known && i<token_filled_cells(data.completed_used[row]) ? GREEN : INACTIVE_GREEN);
    }
}
static void heatmap_draw(lv_event_t *e) {
    lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    int days=overview_days[overview_range],start=TOKEN_HISTORY_DAYS-days;
    uint64_t maximum=0;
    for(int i=start;i<TOKEN_HISTORY_DAYS;i++) if(data.daily_available[i] && data.daily_tokens[i]>maximum) maximum=data.daily_tokens[i];
    const uint32_t palette[]={0x103831,0x07554E,0x087F70,0x00B89B,GREEN};
    int columns=10, stride=22;
    for(int j=0;j<days;j++) {
        int i=start+j,level=token_heat_level(data.daily_tokens[i],maximum,have_data && data.history_available && data.daily_available[i]);
        int x=a.x1+(220-columns*stride+3)/2+(j%columns)*stride,y=a.y1+(j/columns)*20;
        rect(layer,x,y,stride-3,18,level<0?0x242635:palette[level]);
        if(level<0) rect(layer,x+stride/2-2,y+7,2,2,0x65687B);
    }
    /* Scale always uses the same five shades; missing dates have a separate marker. */
    for(int i=0;i<5;i++) rect(layer,a.x1+77+i*14,a.y1+100,11,6,palette[i]);
    rect(layer,a.x1+201,a.y1+100,11,6,0x242635);
    rect(layer,a.x1+205,a.y1+102,2,2,0x65687B);
}
static void set_digits(const char *value) {
    if(strcmp(digits,value)){snprintf(digits,sizeof(digits),"%s",value);lv_obj_invalidate(number);}
}
static void set_progress(int value) {
    if(value!=bar_percent){bar_percent=value;lv_obj_invalidate(bar);}
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int width) {
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_obj_set_style_text_font(o,&lv_font_unscii_8,0);lv_obj_set_style_text_color(o,lv_color_hex(GREEN),0);
    return o;
}
static lv_obj_t *pixel_label(lv_obj_t *parent,int x,int y,int width) {
    lv_obj_t *o=label(parent,x,y,width);lv_obj_set_style_text_font(o,&lv_font_unscii_8,0);return o;
}
static const char *connection_text(void) {
    if(pairing) return "BLE SETUP";
    if(!api_url[0] || !api_key[0]) return "SET API VIA USB";
    if(network_status==5) return "SET UP WI-FI";
    if(!token_wifi_ready()) return network_status==6 ? "CONNECTING" : have_data ? "OFFLINE" : "NO WI-FI";
    if(network_status==1) return "SYNCING";
    if(network_status==2) return "SYNC FAILED";
    if(network_status==3) return "SYNC CLOCK";
    if(network_status==4) return "BLE ACTIVE";
    if(have_data && (cached || data.stale || (esp_timer_get_time()-received_us)/1000000+data.now-data.fetched>1800)) return "CACHED";
    return have_data ? "SYNCED" : "WAITING";
}
static void draw(void) {
    char text[160],value[16],unit[24];int progress=0;
    lv_label_set_text(title,names[page]);
    if(battery<0) lv_label_set_text(power,"--");else lv_label_set_text_fmt(power,"%d%%",battery);
    for(int i=0;i<3;i++) {
        if(page==7) lv_obj_remove_flag(settings_rows[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(settings_rows[i],LV_OBJ_FLAG_HIDDEN);
    }
    if(page==7) { lv_obj_add_flag(detail,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(goal,LV_OBJ_FLAG_HIDDEN); }
    else {lv_obj_remove_flag(detail,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(goal,LV_OBJ_FLAG_HIDDEN);}
    lv_obj_t *overview_objects[]={heatmap,overview_stats,overview_dates,overview_legend};
    lv_obj_t *normal_objects[]={number,unit_label,bar,middle_line};
    for(unsigned i=0;i<4;i++) {
        if(page==8) {lv_obj_remove_flag(overview_objects[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(normal_objects[i],LV_OBJ_FLAG_HIDDEN);}
        else {lv_obj_add_flag(overview_objects[i],LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(normal_objects[i],LV_OBJ_FLAG_HIDDEN);}
    }
    if(page==9) lv_obj_remove_flag(completion_panel,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(completion_panel,LV_OBJ_FLAG_HIDDEN);
    if(page==9) {lv_obj_add_flag(bar,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(detail,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(goal,LV_OBJ_FLAG_HIDDEN);}
    if(page==8) {lv_obj_add_flag(detail,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(goal,LV_OBJ_FLAG_HIDDEN);}
    lv_obj_t *period_objects[]={period_dates,period_coverage,period_latest};
    for(int i=0;i<3;i++) {
        if(page<5) lv_obj_remove_flag(period_objects[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(period_objects[i],LV_OBJ_FLAG_HIDDEN);
    }
    if(page<5) lv_obj_add_flag(detail,LV_OBJ_FLAG_HIDDEN);
    strcpy(value,"--");strcpy(unit,"TOKENS / NO DATA");
    if(page<5) {
        const token_period_t *p=&data.periods[page];
        if(have_data && p->available) {
            token_format_value(p->tokens,value,sizeof(value),unit,sizeof(unit));
            snprintf(text,sizeof(text),"%s / %" PRIu64,unit,p->tokens);lv_label_set_text(unit_label,text);
        } else lv_label_set_text(unit_label,unit);
        if(have_data) snprintf(text,sizeof(text),"[ %.5s > %.5s ]",p->start+5,p->end+5);
        else strcpy(text,"[ --/-- > --/-- ]");
        lv_label_set_text(period_dates,text);
        lv_label_set_text_fmt(period_coverage,"DATA %d/%d",have_data?p->reported_days:0,have_data?p->days:0);
        if(have_data && data.latest[0]) lv_label_set_text_fmt(period_latest,"LAST %.5s",data.latest+5);
        else lv_label_set_text(period_latest,"LAST --/--");
        progress=have_data && p->quota_available?p->quota_percent:0;
        if(have_data && p->quota_available) snprintf(text,sizeof(text),"QUOTA %d%%\nSAMPLES %d/%d DAYS",progress,p->quota_days,p->days);
        else snprintf(text,sizeof(text),"QUOTA --\nNO QUOTA HISTORY");
        lv_label_set_text(goal,text);
    } else if(page==5) {
        if(have_data && data.quota_available) {
            progress=100-data.used;snprintf(value,sizeof(value),"%d",progress);lv_label_set_text(unit_label,"WEEKLY CAPACITY / %");
            time_t reset=data.reset+8*3600;struct tm when;gmtime_r(&reset,&when);
            snprintf(text,sizeof(text),"LEFT THIS WEEK\nRESET %02d-%02d %02d:%02d",when.tm_mon+1,when.tm_mday,when.tm_hour,when.tm_min);lv_label_set_text(detail,text);
            int64_t left=data.reset-data.now-(esp_timer_get_time()-received_us)/1000000;
            if(cached) strcpy(text,"CACHED QUOTA\nSYNC TO UPDATE");
            else if(left<=0) strcpy(text,"RESET DUE\nOK SYNC / UTC+8");
            else snprintf(text,sizeof(text),"RESET IN\n%lldD %lldH %lldM",(long long)(left/86400),(long long)(left/3600%24),(long long)(left/60%60));
            lv_label_set_text(goal,text);
        } else {lv_label_set_text(unit_label,"PERCENT / UNKNOWN");lv_label_set_text(detail,"NO QUOTA DATA\nOK TO SYNC");lv_label_set_text(goal,"QUOTA AND TOKENS\nSEPARATE METRICS");}
    } else if(page==6) {
        if(have_data && data.lifetime_available) token_format_value(data.lifetime,value,sizeof(value),unit,sizeof(unit));
        lv_label_set_text(unit_label,unit);lv_label_set_text(detail,"ALL-TIME TOKENS\nAI EVERY DAY");
        if(have_data && data.streak_available) lv_label_set_text_fmt(goal,"STREAK %d DAYS\nMAKE THINGS WITH AI",data.streak);
        else lv_label_set_text(goal,"STREAK --\nMAKE THINGS WITH AI");
    } else if(page==8) {
        int days=overview_days[overview_range],reported=0,active=0;
        uint64_t total=0,maximum=0;
        for(int i=TOKEN_HISTORY_DAYS-days;i<TOKEN_HISTORY_DAYS;i++) if(have_data && data.history_available && data.daily_available[i]) {
            reported++;total+=data.daily_tokens[i];if(data.daily_tokens[i]) active++;
            if(data.daily_tokens[i]>maximum) maximum=data.daily_tokens[i];
        }
        char total_text[24],peak_text[24];number_text(total_text,sizeof(total_text),total);number_text(peak_text,sizeof(peak_text),maximum);
        if(reported) snprintf(text,sizeof(text),"SUM %s MAX %s\nACTIVE %d DATA %d/%d",total_text,peak_text,active,reported,days);
        else snprintf(text,sizeof(text),"TOTAL -- PEAK --\nDATA 0/%d DAYS",days);
        lv_label_set_text(overview_stats,text);
        if(have_data && data.history_available) snprintf(text,sizeof(text),"%dD / THROUGH %.5s",days,data.history_end+5);
        else snprintf(text,sizeof(text),"%dD / NO DAILY DATA",days);
        lv_label_set_text(overview_dates,text);lv_obj_invalidate(heatmap);
    } else if(page==9) {
        int sum=0,exhausted=0;
        for(int i=0;i<data.completed_week_count;i++) {sum+=data.completed_used[i];if(data.completed_used[i]>=100) exhausted++;}
        if(have_data && data.completed_week_count) {progress=sum/data.completed_week_count;snprintf(value,sizeof(value),"%d",progress);}
        lv_label_set_text_fmt(unit_label,"30D / EXHAUSTED %d/%d",exhausted,have_data?data.completed_week_count:0);
        for(int i=0;i<5;i++) {
            if(have_data && i<data.completed_week_count) {
                time_t end=(time_t)data.completed_reset[i];struct tm tm;localtime_r(&end,&tm);
                snprintf(text,sizeof(text),"%02d-%02d %3d%%",tm.tm_mon+1,tm.tm_mday,data.completed_used[i]);
            } else snprintf(text,sizeof(text),"--/--  --%%");
            snprintf(completion_text[i],sizeof(completion_text[i]),"%.31s",text);
        }
        lv_obj_invalidate(completion_panel);
    } else {
        if(interval) snprintf(value,sizeof(value),"%lu",(unsigned long)interval);
        lv_label_set_text(unit_label,interval?"MIN / AUTO REFRESH":"MANUAL / AUTO REFRESH OFF");
        lv_label_set_text(settings_rows[0],token_wifi_ready()?"WI-FI / CONNECTED":"WI-FI / RECONNECT");
        if(interval) lv_label_set_text_fmt(settings_rows[1],"SYNC / %lu MIN",(unsigned long)interval);
        else lv_label_set_text(settings_rows[1],"SYNC / MANUAL");
        lv_label_set_text(settings_rows[2],pairing?"BLE / ON":"BLE / START");
        for(int i=0;i<3;i++) {
            lv_obj_set_style_bg_color(settings_rows[i],lv_color_hex(i==settings_selection?0x124C44:0x0D0F1B),0);
            lv_obj_set_style_border_color(settings_rows[i],lv_color_hex(i==settings_selection?GREEN:DIM_GREEN),0);
        }
    }
    set_digits(value);set_progress(progress);
    if(page==7) snprintf(text,sizeof(text),"%s / SET",connection_text());
    else snprintf(text,sizeof(text),"%s %02d/09",connection_text(),page<7?page+1:page);
    lv_label_set_text(footer,text);
    if(have_data && data.fetched>0) {
        time_t stamp=data.fetched+8*3600;struct tm when;gmtime_r(&stamp,&when);
        lv_label_set_text_fmt(updated_at,"UPD %02d-%02d %02d:%02d",when.tm_mon+1,when.tm_mday,when.tm_hour,when.tm_min);
    } else lv_label_set_text(updated_at,"UPD --/-- --:--");
}
static void ui_create(void) {
    lv_obj_t *screen=lv_obj_create(NULL);lv_obj_set_style_bg_color(screen,lv_color_hex(0x0D0F1B),0);
    lv_obj_set_style_pad_all(screen,0,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *header=pixel_label(screen,10,12,175);lv_label_set_text(header,"AI COCKPIT");
    power=pixel_label(screen,198,12,32);
    for(int i=0;i<3;i++) {
        lv_obj_t *line=lv_obj_create(screen);lv_obj_remove_style_all(line);lv_obj_set_pos(line,10,i==0?27:i==1?168:286);lv_obj_set_size(line,220,1);
        if(i==1) middle_line=line;
        lv_obj_set_style_bg_color(line,lv_color_hex(DIM_GREEN),0);lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
    }
    title=label(screen,10,37,220);
    number=lv_obj_create(screen);lv_obj_remove_style_all(number);lv_obj_set_pos(number,10,60);lv_obj_set_size(number,220,83);
    lv_obj_remove_flag(number,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(number,digit_draw,LV_EVENT_DRAW_MAIN,NULL);
    unit_label=pixel_label(screen,10,153,220);lv_obj_set_style_text_align(unit_label,LV_TEXT_ALIGN_CENTER,0);
    detail=label(screen,10,176,220);goal=label(screen,10,222,220);
    lv_obj_set_style_text_color(detail,lv_color_hex(DIM_GREEN),0);
    for(int i=0;i<3;i++) {
        settings_rows[i]=label(screen,10,176+i*29,220);lv_obj_set_height(settings_rows[i],25);
        lv_obj_set_style_bg_opa(settings_rows[i],LV_OPA_COVER,0);lv_obj_set_style_border_width(settings_rows[i],1,0);
        lv_obj_set_style_pad_left(settings_rows[i],5,0);lv_obj_set_style_pad_top(settings_rows[i],2,0);
    }
    bar=lv_obj_create(screen);lv_obj_remove_style_all(bar);lv_obj_set_pos(bar,10,274);lv_obj_set_size(bar,220,9);
    lv_obj_add_event_cb(bar,progress_draw,LV_EVENT_DRAW_MAIN,NULL);lv_obj_remove_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
    footer=label(screen,10,292,168);updated_at=pixel_label(screen,10,308,220);
    lv_obj_set_style_text_color(updated_at,lv_color_hex(DIM_GREEN),0);
    lv_obj_t *settings_button=lv_obj_create(screen);lv_obj_remove_style_all(settings_button);
    lv_obj_set_pos(settings_button,194,289);lv_obj_set_size(settings_button,36,15);
    lv_obj_set_style_border_width(settings_button,1,0);lv_obj_set_style_border_color(settings_button,lv_color_hex(DIM_GREEN),0);lv_obj_set_style_radius(settings_button,2,0);
    lv_obj_remove_flag(settings_button,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *settings_label=pixel_label(settings_button,6,3,24);lv_label_set_text(settings_label,"SET");
    overview_stats=label(screen,10,66,220);
    overview_dates=pixel_label(screen,10,128,220);
    overview_legend=pixel_label(screen,10,260,220);lv_label_set_text(overview_legend,"LOW       HIGH     ?");
    heatmap=lv_obj_create(screen);lv_obj_remove_style_all(heatmap);lv_obj_set_pos(heatmap,10,148);lv_obj_set_size(heatmap,220,110);
    lv_obj_remove_flag(heatmap,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(heatmap,heatmap_draw,LV_EVENT_DRAW_MAIN,NULL);
    period_dates=pixel_label(screen,10,181,220);
    period_coverage=label(screen,10,198,110);period_latest=label(screen,142,198,88);
    lv_obj_set_style_text_color(period_latest,lv_color_hex(DIM_GREEN),0);
    completion_panel=lv_obj_create(screen);lv_obj_remove_style_all(completion_panel);
    lv_obj_set_pos(completion_panel,10,176);lv_obj_set_size(completion_panel,220,90);
    lv_obj_remove_flag(completion_panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(completion_panel,completion_bar_draw,LV_EVENT_DRAW_MAIN,NULL);
    lv_screen_load(screen);
}
static void screen_set_awake(bool awake) {
    if(!awake) bsp_display_backlight(0);
    if(!bsp_lvgl_lock(1000)) return;
    lv_timer_t *refresh=lv_display_get_refr_timer(lv_display_get_default());
    if(awake) {lv_timer_resume(refresh);draw();lv_obj_invalidate(lv_screen_active());}
    else lv_timer_pause(refresh);
    esp_err_t err=esp_lcd_panel_disp_on_off(bsp_display_panel(),awake);
    bsp_lvgl_unlock();
    if(err!=ESP_OK) ESP_LOGW(TAG,"Panel state failed: %s",esp_err_to_name(err));
    if(awake) bsp_display_backlight(60);
    ESP_LOGI(TAG,"Screen %s",awake?"ON":"OFF");
}
static bool fetch(token_board_t *out) {
    if (!api_key[0] || time(NULL) < 1735689600) return false;
    esp_http_client_config_t cfg = {.url=api_url, .timeout_ms=12000,
        .user_agent="AICockpit/0.5.0", .crt_bundle_attach=esp_crt_bundle_attach, .disable_auto_redirect=true, .buffer_size=1024};
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return false;
    char auth[128]; snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    esp_http_client_set_header(h, "Authorization", auth);
    bool ok = false;
    static char buffer[6144];
    ESP_LOGI(TAG, "HTTPS heap: free=%u largest=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    esp_err_t open_err = esp_http_client_open(h, 0);
    if (open_err == ESP_OK) {
        esp_http_client_fetch_headers(h);
        ESP_LOGI(TAG, "HTTPS status=%d", esp_http_client_get_status_code(h));
        if (esp_http_client_get_status_code(h) == 200) {
            size_t used = 0; int n = 0;
            while (used < sizeof(buffer)-1 && (n=esp_http_client_read(h, buffer+used, sizeof(buffer)-1-used)) > 0) used += n;
            buffer[used] = 0;
            if (n >= 0 && esp_http_client_is_complete_data_received(h)) ok = token_board_parse(buffer, out);
        }
        esp_http_client_close(h);
    } else {
        ESP_LOGW(TAG, "HTTPS open failed: %s", esp_err_to_name(open_err));
    }
    ESP_LOGI(TAG, "HTTPS payload validation=%s", ok ? "PASS" : "FAIL");
    esp_http_client_cleanup(h); return ok;
}
static void network_worker(void *arg) {
    (void)arg;
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    sntp.start = false;
    bool sntp_started = false;
    esp_err_t ntp_error = esp_netif_sntp_init(&sntp);
    int64_t due = 0;
    for (;;) {
        uint32_t request=0;
        xTaskNotifyWait(0,UINT32_MAX,&request,pdMS_TO_TICKS(1000));
        int64_t now = esp_timer_get_time()/1000000;
        if(request&NET_RECONNECT) {
            esp_err_t err=token_wifi_reconnect();
            update_t update={.status=err==ESP_OK?6:5};xQueueOverwrite(updates,&update);
            due=0;
        }
        if (!token_wifi_https_allowed()) {
            update_t update = {.status=4}; xQueueOverwrite(updates, &update); continue;
        }
        if (!token_wifi_ready()) {
            if (now%15 == 0) token_wifi_retry();
            continue;
        }
        if (!sntp_started && ntp_error == ESP_OK) {
            sntp_started = esp_netif_sntp_start() == ESP_OK;
        }
        if (time(NULL) < 1735689600) {
            update_t update = {.status=3}; xQueueOverwrite(updates, &update); continue;
        }
        if (!(request&NET_REFRESH) && now < due) continue;
        update_t update = {.status=1}; xQueueOverwrite(updates, &update);
        token_wifi_https_busy(true);
        update.status = fetch(&update.board) ? 0 : 2;
        token_wifi_https_busy(false);
        xQueueOverwrite(updates, &update);
        uint32_t minutes = interval;
        due = now + (minutes ? minutes*60 : INT32_MAX);
    }
}
static void key_callback(bsp_btn_t button, bsp_btn_ev_t event, void *user) {
    (void)user;
    token_key_event_t key = {button, event}; xQueueSend(keys, &key, 0);
}
static void save_preferences(void) {
    nvs_handle_t h;
    if (nvs_open("token_board", NVS_READWRITE, &h) != ESP_OK) return;
    esp_err_t err = nvs_set_blob(h, "targets", targets, sizeof(targets));
    if (err == ESP_OK) err = nvs_set_u32(h, "interval", interval);
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "Preferences not saved: %s", esp_err_to_name(err));
    nvs_close(h);
}
static void enter_settings(void) {
    return_page=page!=7?page:8;page=7;settings_selection=0;
}
static void process_key(token_key_event_t key) {
    uint64_t now_ms=(uint64_t)esp_timer_get_time()/1000;
    token_input_result_t result=token_power_input(&screen_power,key.button==BSP_BTN_OK,now_ms);
    if(result==TOKEN_INPUT_WAKE) {screen_set_awake(true);return;}
    if(result==TOKEN_INPUT_IGNORE || key.event==BSP_BTN_PRESS) return;
    if(page==7) {
        if(key.event==BSP_BTN_LONG && key.button==BSP_BTN_OK) {page=return_page;return;}
        if(key.event==BSP_BTN_LONG && key.button==BSP_BTN_DOWN) {
            token_wifi_clear();pairing=true;pair_deadline=esp_timer_get_time()+300000000;token_wifi_pair(true);return;
        }
        if(key.event!=BSP_BTN_CLICK) return;
        if(key.button==BSP_BTN_UP) settings_selection=(settings_selection+2)%3;
        else if(key.button==BSP_BTN_DOWN) settings_selection=(settings_selection+1)%3;
        else if(key.button==BSP_BTN_OK) {
            if(settings_selection==0) xTaskNotify(net_task,NET_RECONNECT|NET_REFRESH,eSetBits);
            else if(settings_selection==1) {
                for(unsigned i=0;i<5;i++) if(interval==intervals[i]) {interval=intervals[(i+1)%5];break;}
                save_preferences();xTaskNotify(net_task,NET_REFRESH,eSetBits);
            } else {
                pairing=!pairing;pair_deadline=esp_timer_get_time()+300000000;token_wifi_pair(pairing);
            }
        }
        return;
    }
    if(key.button==BSP_BTN_OK && key.event==BSP_BTN_LONG) {enter_settings();return;}
    if(page==8 && key.button==BSP_BTN_OK) {
        if(key.event==BSP_BTN_CLICK) overview_range=(overview_range+1)%3;
        return;
    }
    if(key.event!=BSP_BTN_CLICK) return;
    if(editing) {
        if(key.button==BSP_BTN_UP && edit_target<=UINT64_C(1000000000000)-1000000) edit_target+=1000000;
        if(key.button==BSP_BTN_DOWN && edit_target>=1000000) edit_target-=1000000;
    } else if(key.button==BSP_BTN_UP || key.button==BSP_BTN_DOWN) {
        page=token_navigation_next(page,key.button==BSP_BTN_UP);
    } else if(key.button==BSP_BTN_OK) xTaskNotify(net_task,NET_REFRESH,eSetBits);
}
void app_main(void) {
    ESP_LOGI(TAG, "AI Cockpit 0.5.0 Community");
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_display_init());
    bsp_display_backlight(0);
    if (!bsp_lvgl_init()) { ESP_LOGE(TAG, "Display unavailable"); return; }
    bsp_battery_init();
    token_power_init(&screen_power);
    if (token_wifi_start() != ESP_OK) ESP_LOGW(TAG, "Network setup failed");
    nvs_handle_t h;
    if (nvs_open("token_board", NVS_READONLY, &h) == ESP_OK) {
        char configured_url[sizeof(api_url)]={0},configured_key[sizeof(api_key)]={0};
        size_t url_size=sizeof(configured_url),key_size=sizeof(configured_key);
        if(nvs_get_str(h,"api_url",configured_url,&url_size)==ESP_OK &&
           nvs_get_str(h,"api_key",configured_key,&key_size)==ESP_OK &&
           !strncmp(configured_url,"https://",8) && strlen(configured_key)>=32 &&
           !strpbrk(configured_url,"\r\n") && !strpbrk(configured_key,"\r\n")) {
            memcpy(api_url,configured_url,sizeof(api_url));memcpy(api_key,configured_key,sizeof(api_key));
        }
        size_t size=sizeof(targets); nvs_get_blob(h, "targets", targets, &size);
        uint32_t minutes;
        if (nvs_get_u32(h,"interval",&minutes)==ESP_OK) for (unsigned i=0;i<5;i++) if (minutes==intervals[i]) interval=minutes;
        size=0;
        if (nvs_get_blob(h,"snapshot",NULL,&size)==ESP_OK && size==sizeof(data) &&
            nvs_get_blob(h,"snapshot",&data,&size)==ESP_OK) { have_data=true; cached=true; }
        nvs_close(h);
    }
    keys=xQueueCreate(8,sizeof(token_key_event_t)); updates=xQueueCreate(1,sizeof(update_t));
    if (!keys || !updates) { ESP_LOGE(TAG,"Queue allocation failed"); return; }
    if (!bsp_lvgl_lock(1000)) return;
    ui_create(); draw(); bsp_lvgl_unlock();
    screen_set_awake(false);
    if (xTaskCreate(network_worker,"token_fetch",10240,NULL,4,&net_task)!=pdPASS) { ESP_LOGE(TAG,"Network task allocation failed"); return; }
    ESP_ERROR_CHECK(bsp_button_init(key_callback,NULL));
    int64_t last_battery=0,last_draw=0;
    for (;;) {
        token_key_event_t key; update_t update;
        bool dirty=false;
        if (xQueueReceive(keys,&key,pdMS_TO_TICKS(200))) {process_key(key);dirty=true;}
        if (xQueueReceive(updates,&update,0)) {
            dirty=true;
            network_status=update.status;
            if (update.status==0) {
                data=update.board; have_data=true; cached=false; received_us=esp_timer_get_time();
                if (nvs_open("token_board",NVS_READWRITE,&h)==ESP_OK) {
                    if (nvs_set_blob(h,"snapshot",&data,sizeof(data))==ESP_OK) nvs_commit(h);
                    nvs_close(h);
                }
            }
        }
        int64_t now=esp_timer_get_time();
        token_wifi_poll();
        if (pairing && now>pair_deadline) { token_wifi_pair(false); pairing=false; }
        if (now-last_battery>30000000 || !last_battery) { battery=bsp_battery_soc(); last_battery=now; }
        if(token_power_tick(&screen_power,(uint64_t)now/1000)) screen_set_awake(false);
        if(screen_power.awake && (dirty || now-last_draw>=1000000) && bsp_lvgl_lock(100)) {
            draw();bsp_lvgl_unlock();last_draw=now;
        }
    }
}
