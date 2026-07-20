/*
 * Minimal public cJSON 1.7.x declarations used by this project.
 * Compatible with the upstream cJSON ABI. The linked cJSON library remains
 * under its own MIT license; see THIRD_PARTY_NOTICES.md.
 */
#ifndef LEAD_PROCESSOR_CJSON_COMPAT_H
#define LEAD_PROCESSOR_CJSON_COMPAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int cJSON_bool;

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

#define cJSON_Invalid (0)
#define cJSON_False   (1 << 0)
#define cJSON_True    (1 << 1)
#define cJSON_NULL    (1 << 2)
#define cJSON_Number  (1 << 3)
#define cJSON_String  (1 << 4)
#define cJSON_Array   (1 << 5)
#define cJSON_Object  (1 << 6)

cJSON *cJSON_Parse(const char *value);
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated);
void cJSON_Delete(cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);
void cJSON_free(void *object);

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
int cJSON_GetArraySize(const cJSON *array);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);

cJSON_bool cJSON_IsInvalid(const cJSON * const item);
cJSON_bool cJSON_IsFalse(const cJSON * const item);
cJSON_bool cJSON_IsTrue(const cJSON * const item);
cJSON_bool cJSON_IsBool(const cJSON * const item);
cJSON_bool cJSON_IsNull(const cJSON * const item);
cJSON_bool cJSON_IsNumber(const cJSON * const item);
cJSON_bool cJSON_IsString(const cJSON * const item);
cJSON_bool cJSON_IsArray(const cJSON * const item);
cJSON_bool cJSON_IsObject(const cJSON * const item);

cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);
cJSON *cJSON_CreateBool(cJSON_bool boolean);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);

cJSON_bool cJSON_AddItemToArray(cJSON *array, cJSON *item);
cJSON_bool cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name);
cJSON *cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean);
cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);

#ifdef __cplusplus
}
#endif
#endif
