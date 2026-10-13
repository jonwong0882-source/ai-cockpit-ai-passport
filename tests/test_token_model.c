#include "token_model.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    char buffer[6144]; size_t n = fread(buffer,1,sizeof(buffer)-1,f);fclose(f);buffer[n]=0;
    token_board_t data; assert(token_board_parse(buffer,&data));
    assert(data.periods[0].tokens==30 && data.periods[1].tokens==20);
    assert(!data.quota_available && !data.lifetime_available);
    assert(token_progress(UINT64_MAX-1,UINT64_MAX)==99);
    assert(token_progress(30,20)==100 && token_progress(30,0)==0);
    for (uint64_t target=1; target<200; target++)
        for (uint64_t tokens=0; tokens<target; tokens++)
            assert(token_progress(tokens,target)==(int)(tokens*100/target));
    assert(data.history_available && !data.daily_available[0]);
    assert(data.daily_available[48] && data.daily_tokens[48]==10);
    assert(data.daily_available[49] && data.daily_tokens[49]==20);
    assert(token_heat_level(0,100,false)==-1 && token_heat_level(0,0,true)==0);
    assert(token_heat_level(1,100,true)==1 && token_heat_level(25,100,true)==2);
    assert(token_heat_level(50,100,true)==3 && token_heat_level(75,100,true)==4);
    assert(token_heat_level(UINT64_MAX-1,UINT64_MAX,true)==4);
    for(uint64_t n=1;n<=100;n++) assert(token_heat_level(n,100,true)>=token_heat_level(n-1,100,true));
    token_board_t before=data;
    assert(!token_board_parse("{",&data)); assert(!memcmp(&before,&data,sizeof(data)));
    cJSON *root=cJSON_Parse(buffer);assert(root);
    cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"periods"),0);
    cJSON_ReplaceItemInObject(row,"tokens",cJSON_CreateNumber(-1));
    char *invalid=cJSON_PrintUnformatted(root);assert(invalid);
    assert(!token_board_parse(invalid,&data));assert(!memcmp(&before,&data,sizeof(data)));
    free(invalid);cJSON_Delete(root);
    root=cJSON_Parse(buffer);assert(root);
    cJSON *history=cJSON_GetObjectItem(root,"history");
    cJSON *tokens=cJSON_GetObjectItem(history,"tokens");
    cJSON_ReplaceItemInArray(tokens,0,cJSON_CreateNumber(-1));
    invalid=cJSON_PrintUnformatted(root);assert(invalid);
    assert(!token_board_parse(invalid,&data));assert(!memcmp(&before,&data,sizeof(data)));
    free(invalid);
    cJSON_DeleteItemFromArray(tokens,0);
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(!token_board_parse(invalid,&data));free(invalid);
    cJSON_DeleteItemFromObject(root,"history");
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(token_board_parse(invalid,&data));assert(!data.history_available);
    free(invalid);cJSON_Delete(root);
    root=cJSON_Parse(buffer);assert(root);
    cJSON *completion=cJSON_AddObjectToObject(root,"completion");
    cJSON *weeks=cJSON_AddArrayToObject(completion,"weeks");
    cJSON *week=cJSON_CreateObject();cJSON_AddItemToArray(weeks,week);
    cJSON_AddNumberToObject(week,"reset",before.now-100);cJSON_AddNumberToObject(week,"used",100);
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(token_board_parse(invalid,&data));
    assert(data.completed_week_count==1 && data.completed_used[0]==100);free(invalid);
    cJSON_ReplaceItemInObject(week,"reset",cJSON_CreateNumber(before.now+1));
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(!token_board_parse(invalid,&data));free(invalid);
    cJSON_Delete(root);
    root=cJSON_Parse(buffer);assert(root);
    row=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"periods"),0);
    cJSON_AddNumberToObject(row,"quota_percent",62.5);cJSON_AddNumberToObject(row,"quota_reported_days",3);
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(token_board_parse(invalid,&data));
    assert(data.periods[0].quota_available && data.periods[0].quota_percent==62 && data.periods[0].quota_days==3);free(invalid);
    cJSON_ReplaceItemInObject(row,"quota_percent",cJSON_CreateNumber(-1));
    invalid=cJSON_PrintUnformatted(root);assert(invalid);assert(!token_board_parse(invalid,&data));free(invalid);cJSON_Delete(root);
    puts("Token model/parser and daily heat scale: PASS");
}
