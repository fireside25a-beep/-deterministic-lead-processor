#include "config.h"
#include "cJSON.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if ((error != NULL) && (error_size > 0U)) {
        (void)lead_copy_string(error, error_size, message);
    }
}

static char *read_config_file(const char *path, size_t *length, char *error, size_t error_size) {
    FILE *file = NULL;
    char *data = NULL;
    long file_size = 0L;
    size_t read_count = 0U;
    char message[LEAD_ERROR_MAX];

    file = fopen(path, "rb");
    if (file == NULL) {
        (void)snprintf(message, sizeof(message), "cannot open config: %s", strerror(errno));
        set_error(error, error_size, message);
        return NULL;
    }
    if ((fseek(file, 0L, SEEK_END) != 0) || ((file_size = ftell(file)) < 0L) ||
        (fseek(file, 0L, SEEK_SET) != 0)) {
        set_error(error, error_size, "cannot inspect config file");
        (void)fclose(file);
        return NULL;
    }
    if (file_size > 1024L * 1024L) {
        set_error(error, error_size, "config exceeds 1 MiB limit");
        (void)fclose(file);
        return NULL;
    }
    data = (char *)malloc((size_t)file_size + 1U);
    if (data == NULL) {
        set_error(error, error_size, "out of memory reading config");
        (void)fclose(file);
        return NULL;
    }
    read_count = fread(data, 1U, (size_t)file_size, file);
    if ((read_count != (size_t)file_size) || ferror(file)) {
        set_error(error, error_size, "cannot read config file");
        free(data);
        (void)fclose(file);
        return NULL;
    }
    data[read_count] = '\0';
    *length = read_count;
    (void)fclose(file);
    return data;
}

static void add_city(LeadConfig *config, const char *city) {
    if ((config->service_area_city_count < CONFIG_MAX_CITIES) && (city != NULL)) {
        (void)lead_copy_string(config->service_area_cities[config->service_area_city_count],
                               LEAD_CITY_MAX, city);
        ++config->service_area_city_count;
    }
}

static bool add_price(LeadConfig *config, const char *space, double min_rate,
                      double max_rate, double urgent_adjustment) {
    PriceRule *rule = NULL;
    if ((config == NULL) || (config->price_count >= CONFIG_MAX_PRICES) ||
        (space == NULL) || !isfinite(min_rate) || !isfinite(max_rate) ||
        !isfinite(urgent_adjustment) || (min_rate < 0.0) ||
        (max_rate < min_rate) || (urgent_adjustment < 0.0)) {
        return false;
    }
    rule = &config->prices[config->price_count];
    memset(rule, 0, sizeof(*rule));
    if (!lead_copy_string(rule->space, sizeof(rule->space), space)) {
        return false;
    }
    rule->min_per_sqft = min_rate;
    rule->max_per_sqft = max_rate;
    rule->urgent_max_adjustment = urgent_adjustment;
    ++config->price_count;
    return true;
}

static void add_keyword(LeadConfig *config, const char *keyword) {
    if ((config->high_urgency_keyword_count < CONFIG_MAX_KEYWORDS) && (keyword != NULL)) {
        (void)lead_copy_string(config->high_urgency_keywords[config->high_urgency_keyword_count],
                               CONFIG_KEYWORD_MAX, keyword);
        ++config->high_urgency_keyword_count;
    }
}

void lead_config_defaults(LeadConfig *config) {
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->unknown_city_in_area_default = true;
    config->max_model_retries = 2U;
    config->request_timeout_seconds = 15L;
    config->max_response_bytes = 65536U;

    add_city(config, "Vancouver");
    add_city(config, "Surrey");
    add_city(config, "Richmond");
    add_city(config, "Burnaby");
    add_city(config, "Coquitlam");
    add_city(config, "White Rock");
    add_city(config, "North Vancouver");
    add_city(config, "Delta");
    add_city(config, "Langley");
    add_city(config, "Port Moody");

    (void)add_price(config, "garage", 7.40, 8.60, 50.0);
    (void)add_price(config, "basement", 12.00, 14.00, 0.0);
    (void)add_price(config, "driveway", 9.20, 10.80, 0.0);
    (void)add_price(config, "commercial", 11.00, 13.00, 0.0);
    (void)add_price(config, "patio", 13.80, 16.20, 0.0);
    (void)add_price(config, "interior", 11.05, 12.95, 0.0);

    add_keyword(config, "asap");
    add_keyword(config, "urgent");
    add_keyword(config, "urgently");
    add_keyword(config, "today");
    add_keyword(config, "water everywhere");
    add_keyword(config, "before august");
    add_keyword(config, "maintenance week");
    add_keyword(config, "in 3 weeks");
    add_keyword(config, "this week");
}

static bool load_string_array(cJSON *array, char destination[][LEAD_CITY_MAX],
                              size_t max_count, size_t *count) {
    int i = 0;
    int size = 0;
    if (!cJSON_IsArray(array)) {
        return false;
    }
    size = cJSON_GetArraySize(array);
    *count = 0U;
    for (i = 0; i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsString(item) || (item->valuestring == NULL) || (*count >= max_count)) {
            return false;
        }
        if (!lead_copy_string(destination[*count], LEAD_CITY_MAX, item->valuestring)) {
            return false;
        }
        ++(*count);
    }
    return true;
}

static bool json_number_is_integer_in_range(const cJSON *item, double minimum, double maximum) {
    double value = 0.0;
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    value = item->valuedouble;
    return isfinite(value) && (value >= minimum) && (value <= maximum) &&
           (floor(value) == value);
}

bool lead_config_load_json(const char *path, LeadConfig *config, char *error, size_t error_size) {
    char *data = NULL;
    size_t length = 0U;
    const char *end = NULL;
    cJSON *root = NULL;
    cJSON *item = NULL;
    cJSON *prices = NULL;
    cJSON *urgency = NULL;
    int i = 0;
    int count = 0;
    LeadConfig loaded;

    if ((path == NULL) || (config == NULL)) {
        set_error(error, error_size, "invalid config arguments");
        return false;
    }
    lead_config_defaults(&loaded);
    data = read_config_file(path, &length, error, error_size);
    (void)length;
    if (data == NULL) {
        return false;
    }
    root = cJSON_ParseWithOpts(data, &end, 1);
    free(data);
    if ((root == NULL) || !cJSON_IsObject(root)) {
        set_error(error, error_size, "config must be exactly one JSON object");
        cJSON_Delete(root);
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "service_area_cities");
    if (item != NULL) {
        if (!load_string_array(item, loaded.service_area_cities, CONFIG_MAX_CITIES,
                               &loaded.service_area_city_count)) {
            set_error(error, error_size, "service_area_cities must be a bounded string array");
            cJSON_Delete(root);
            return false;
        }
    }
    item = cJSON_GetObjectItemCaseSensitive(root, "unknown_city_in_area_default");
    if (item != NULL) {
        if (!cJSON_IsBool(item)) {
            set_error(error, error_size, "unknown_city_in_area_default must be boolean");
            cJSON_Delete(root);
            return false;
        }
        loaded.unknown_city_in_area_default = cJSON_IsTrue(item);
    }
    item = cJSON_GetObjectItemCaseSensitive(root, "max_model_retries");
    if (item != NULL) {
        if (!json_number_is_integer_in_range(item, 0.0, 10.0)) {
            set_error(error, error_size, "max_model_retries must be between 0 and 10");
            cJSON_Delete(root);
            return false;
        }
        loaded.max_model_retries = (unsigned int)item->valueint;
    }
    item = cJSON_GetObjectItemCaseSensitive(root, "request_timeout_seconds");
    if (item != NULL) {
        if (!json_number_is_integer_in_range(item, 1.0, 300.0)) {
            set_error(error, error_size, "request_timeout_seconds must be between 1 and 300");
            cJSON_Delete(root);
            return false;
        }
        loaded.request_timeout_seconds = (long)item->valueint;
    }
    item = cJSON_GetObjectItemCaseSensitive(root, "max_response_bytes");
    if (item != NULL) {
        if (!json_number_is_integer_in_range(item, 1024.0, 4.0 * 1024.0 * 1024.0)) {
            set_error(error, error_size, "max_response_bytes must be between 1024 and 4194304");
            cJSON_Delete(root);
            return false;
        }
        loaded.max_response_bytes = (size_t)item->valuedouble;
    }

    prices = cJSON_GetObjectItemCaseSensitive(root, "prices");
    if (prices != NULL) {
        if (!cJSON_IsArray(prices)) {
            set_error(error, error_size, "prices must be an array");
            cJSON_Delete(root);
            return false;
        }
        loaded.price_count = 0U;
        count = cJSON_GetArraySize(prices);
        for (i = 0; i < count; ++i) {
            cJSON *entry = cJSON_GetArrayItem(prices, i);
            cJSON *space = cJSON_GetObjectItemCaseSensitive(entry, "space");
            cJSON *min_rate = cJSON_GetObjectItemCaseSensitive(entry, "min_per_sqft");
            cJSON *max_rate = cJSON_GetObjectItemCaseSensitive(entry, "max_per_sqft");
            cJSON *urgent_adjustment = cJSON_GetObjectItemCaseSensitive(entry, "urgent_max_adjustment");
            if (!cJSON_IsObject(entry) || !cJSON_IsString(space) || !cJSON_IsNumber(min_rate) ||
                !cJSON_IsNumber(max_rate) || (space->valuestring == NULL) ||
                !isfinite(min_rate->valuedouble) || !isfinite(max_rate->valuedouble) ||
                (min_rate->valuedouble < 0.0) || (max_rate->valuedouble < min_rate->valuedouble) ||
                (loaded.price_count >= CONFIG_MAX_PRICES) ||
                ((urgent_adjustment != NULL) &&
                 (!cJSON_IsNumber(urgent_adjustment) || !isfinite(urgent_adjustment->valuedouble) ||
                  (urgent_adjustment->valuedouble < 0.0)))) {
                set_error(error, error_size, "each price needs bounded non-negative rates and an optional non-negative urgent adjustment");
                cJSON_Delete(root);
                return false;
            }
            if (!add_price(&loaded, space->valuestring, min_rate->valuedouble,
                           max_rate->valuedouble,
                           urgent_adjustment != NULL ? urgent_adjustment->valuedouble : 0.0)) {
                set_error(error, error_size, "price entry exceeds configured limits");
                cJSON_Delete(root);
                return false;
            }
        }
    }

    urgency = cJSON_GetObjectItemCaseSensitive(root, "high_urgency_keywords");
    if (urgency != NULL) {
        int size = 0;
        loaded.high_urgency_keyword_count = 0U;
        if (!cJSON_IsArray(urgency)) {
            set_error(error, error_size, "high_urgency_keywords must be an array");
            cJSON_Delete(root);
            return false;
        }
        size = cJSON_GetArraySize(urgency);
        for (i = 0; i < size; ++i) {
            cJSON *keyword = cJSON_GetArrayItem(urgency, i);
            if (!cJSON_IsString(keyword) || (keyword->valuestring == NULL) ||
                (loaded.high_urgency_keyword_count >= CONFIG_MAX_KEYWORDS) ||
                !lead_copy_string(loaded.high_urgency_keywords[loaded.high_urgency_keyword_count],
                                  CONFIG_KEYWORD_MAX, keyword->valuestring)) {
                set_error(error, error_size, "high_urgency_keywords contains an invalid entry");
                cJSON_Delete(root);
                return false;
            }
            ++loaded.high_urgency_keyword_count;
        }
    }

    *config = loaded;
    cJSON_Delete(root);
    return true;
}

static int ascii_casecmp(const char *a, const char *b) {
    unsigned char ca = 0U;
    unsigned char cb = 0U;
    while ((*a != '\0') && (*b != '\0')) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if ((ca >= 'A') && (ca <= 'Z')) {
            ca = (unsigned char)(ca + ('a' - 'A'));
        }
        if ((cb >= 'A') && (cb <= 'Z')) {
            cb = (unsigned char)(cb + ('a' - 'A'));
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

const PriceRule *lead_config_find_price(const LeadConfig *config, const char *space) {
    size_t i = 0U;
    if ((config == NULL) || (space == NULL)) {
        return NULL;
    }
    for (i = 0U; i < config->price_count; ++i) {
        if (ascii_casecmp(config->prices[i].space, space) == 0) {
            return &config->prices[i];
        }
    }
    return NULL;
}
