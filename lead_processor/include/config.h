#ifndef LEAD_PROCESSOR_CONFIG_H
#define LEAD_PROCESSOR_CONFIG_H

#include "lead.h"

#include <stdbool.h>
#include <stddef.h>

#define CONFIG_MAX_CITIES 64U
#define CONFIG_MAX_PRICES 32U
#define CONFIG_MAX_KEYWORDS 64U
#define CONFIG_KEYWORD_MAX 96U

typedef struct {
    char space[LEAD_SPACE_MAX];
    double min_per_sqft;
    double max_per_sqft;
    double urgent_max_adjustment;
} PriceRule;

typedef struct {
    char service_area_cities[CONFIG_MAX_CITIES][LEAD_CITY_MAX];
    size_t service_area_city_count;
    bool unknown_city_in_area_default;

    PriceRule prices[CONFIG_MAX_PRICES];
    size_t price_count;

    char high_urgency_keywords[CONFIG_MAX_KEYWORDS][CONFIG_KEYWORD_MAX];
    size_t high_urgency_keyword_count;

    unsigned int max_model_retries;
    long request_timeout_seconds;
    size_t max_response_bytes;
} LeadConfig;

void lead_config_defaults(LeadConfig *config);
bool lead_config_load_json(const char *path, LeadConfig *config, char *error, size_t error_size);
const PriceRule *lead_config_find_price(const LeadConfig *config, const char *space);

#endif
