#include "token_model.h"
#include "cJSON.h"
#include <math.h>
#include <string.h>
static bool integer(const cJSON *v, double max) {
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= 0 &&
           v->valuedouble <= max && floor(v->valuedouble) == v->valuedouble;
}
static bool date(const cJSON *v, char out[11]) {
    if (!cJSON_IsString(v) || strlen(v->valuestring) != 10) return false;
    const char *s = v->valuestring;
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) { if (s[i] != '-') return false; }
        else if (s[i] < '0' || s[i] > '9') return false;
    }
    memcpy(out, s, 11); return true;
}
int token_progress(uint64_t tokens, uint64_t target) {
    if (!target) return 0;
    if (tokens >= target) return 100;
    for (unsigned p = 99; p > 0; --p) {
        uint64_t threshold = target / 100 * p + (target % 100 * p + 99) / 100;
        if (tokens >= threshold) return (int)p;
    }
    return 0;
}
bool token_board_parse(const char *json, token_board_t *output) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    token_board_t b = {0};
    bool ok = false;
    const char *keys[] = {"recent_week", "yesterday", "last_week", "last_month", "month_to_date"};
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!integer(v, 1) || v->valueint != 1) goto end;
    cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "periods");
    if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) != 5) goto end;
    for (int i = 0; i < 5; ++i) {
        cJSON *r = cJSON_GetArrayItem(rows, i);
        v = cJSON_GetObjectItemCaseSensitive(r, "key");
        if (!cJSON_IsString(v) || strcmp(v->valuestring, keys[i])) goto end;
        if (!date(cJSON_GetObjectItemCaseSensitive(r, "start"), b.periods[i].start) ||
            !date(cJSON_GetObjectItemCaseSensitive(r, "end"), b.periods[i].end)) goto end;
        v = cJSON_GetObjectItemCaseSensitive(r, "tokens");
        if (!cJSON_IsNull(v) && !integer(v, 9007199254740991.0)) goto end;
        b.periods[i].available = !cJSON_IsNull(v);
        if (b.periods[i].available) b.periods[i].tokens = (uint64_t)v->valuedouble;
        v = cJSON_GetObjectItemCaseSensitive(r, "days");
        if (!integer(v, 31) || v->valueint < 1) goto end;
        b.periods[i].days = v->valueint;
        v = cJSON_GetObjectItemCaseSensitive(r, "reported_days");
        if (!integer(v, b.periods[i].days)) goto end;
        b.periods[i].reported_days = v->valueint;
        cJSON *quota=cJSON_GetObjectItemCaseSensitive(r,"quota_percent");
        cJSON *coverage=cJSON_GetObjectItemCaseSensitive(r,"quota_reported_days");
        if(quota || coverage) {
            if(!integer(coverage,b.periods[i].days) || (!cJSON_IsNull(quota) &&
               (!cJSON_IsNumber(quota) || !isfinite(quota->valuedouble) || quota->valuedouble<0 || quota->valuedouble>100000))) goto end;
            b.periods[i].quota_days=coverage->valueint;
            b.periods[i].quota_available=cJSON_IsNumber(quota);
            if(b.periods[i].quota_available) b.periods[i].quota_percent=(int)quota->valuedouble;
        }
    }
    v = cJSON_GetObjectItemCaseSensitive(root, "latest");
    if (!cJSON_IsNull(v) && !date(v, b.latest)) goto end;
    v = cJSON_GetObjectItemCaseSensitive(root, "now");
    if (!integer(v, 4102444800.0)) goto end;
    b.now = (int64_t)v->valuedouble;
    v = cJSON_GetObjectItemCaseSensitive(root, "fetched_at");
    if (!integer(v, 4102444800.0)) goto end;
    b.fetched = (int64_t)v->valuedouble;
    v = cJSON_GetObjectItemCaseSensitive(root, "stale");
    if (!cJSON_IsBool(v)) goto end;
    b.stale = cJSON_IsTrue(v);
    v = cJSON_GetObjectItemCaseSensitive(root, "lifetime");
    b.lifetime_available = integer(v, 9007199254740991.0);
    if (b.lifetime_available) b.lifetime = (uint64_t)v->valuedouble;
    v = cJSON_GetObjectItemCaseSensitive(root, "streak");
    b.streak_available = integer(v, 100000);
    if (b.streak_available) b.streak = v->valueint;
    cJSON *windows = cJSON_GetObjectItemCaseSensitive(root, "windows");
    if (!cJSON_IsArray(windows)) goto end;
    cJSON *w;
    cJSON_ArrayForEach(w, windows) {
        cJSON *bucket = cJSON_GetObjectItemCaseSensitive(w, "bucket");
        cJSON *minutes = cJSON_GetObjectItemCaseSensitive(w, "minutes");
        cJSON *used = cJSON_GetObjectItemCaseSensitive(w, "used");
        cJSON *reset = cJSON_GetObjectItemCaseSensitive(w, "reset");
        if (cJSON_IsString(bucket) && !strcmp(bucket->valuestring, "codex") &&
            integer(minutes, 10080) && minutes->valueint == 10080 &&
            cJSON_IsNumber(used) && isfinite(used->valuedouble) &&
            used->valuedouble >= 0 && used->valuedouble <= 100 && integer(reset, 4102444800.0)) {
            b.quota_available = true; b.used = (int)used->valuedouble;
            b.reset = (int64_t)reset->valuedouble;
        }
    }
    cJSON *history = cJSON_GetObjectItemCaseSensitive(root, "history");
    if (history) {
        if (!cJSON_IsObject(history) ||
            !date(cJSON_GetObjectItemCaseSensitive(history,"start"),b.history_start) ||
            !date(cJSON_GetObjectItemCaseSensitive(history,"end"),b.history_end) ||
            strcmp(b.history_start,b.history_end)>=0) goto end;
        cJSON *tokens=cJSON_GetObjectItemCaseSensitive(history,"tokens");
        if (!cJSON_IsArray(tokens) || cJSON_GetArraySize(tokens)!=TOKEN_HISTORY_DAYS) goto end;
        for (int i=0;i<TOKEN_HISTORY_DAYS;i++) {
            v=cJSON_GetArrayItem(tokens,i);
            if (!cJSON_IsNull(v) && !integer(v,9007199254740991.0)) goto end;
            b.daily_available[i]=!cJSON_IsNull(v);
            if (b.daily_available[i]) b.daily_tokens[i]=(uint64_t)v->valuedouble;
        }
        b.history_available=true;
    }
    cJSON *completion=cJSON_GetObjectItemCaseSensitive(root,"completion");
    if(completion) {
        cJSON *weeks=cJSON_GetObjectItemCaseSensitive(completion,"weeks");
        if(!cJSON_IsArray(weeks) || cJSON_GetArraySize(weeks)>5) goto end;
        b.completed_week_count=cJSON_GetArraySize(weeks);
        for(int i=0;i<b.completed_week_count;i++) {
            cJSON *row=cJSON_GetArrayItem(weeks,i);
            cJSON *reset=cJSON_GetObjectItemCaseSensitive(row,"reset");
            cJSON *used=cJSON_GetObjectItemCaseSensitive(row,"used");
            if(!integer(reset,4102444800.0) || reset->valuedouble>b.now || reset->valuedouble<=b.now-30*86400 ||
               (i && reset->valuedouble<=b.completed_reset[i-1]) ||
               !cJSON_IsNumber(used) || !isfinite(used->valuedouble) || used->valuedouble<0 || used->valuedouble>100) goto end;
            b.completed_reset[i]=(int64_t)reset->valuedouble;
            b.completed_used[i]=(int)used->valuedouble;
        }
    }
    *output = b; ok = true;
end:
    cJSON_Delete(root); return ok;
}

int token_heat_level(uint64_t tokens, uint64_t maximum, bool available) {
    if (!available) return -1;
    if (!tokens) return 0;
    if (!maximum || tokens>=maximum) return 4;
    /* Equal-width bins relative to the maximum; multiplication cannot overflow. */
    for (int p=3;p>=1;p--) {
        uint64_t threshold=maximum/4*p+(maximum%4*p+3)/4;
        if (tokens>=threshold) return p+1;
    }
    return 1;
}
