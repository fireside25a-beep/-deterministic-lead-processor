#include "config.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    LeadConfig config;
    const PriceRule *garage = NULL;
    char error[LEAD_ERROR_MAX] = {0};

    lead_config_defaults(&config);
    CHECK(config.service_area_city_count == 10U);
    CHECK(config.max_model_retries == 2U);
    CHECK(config.request_timeout_seconds == 15L);
    CHECK(config.max_response_bytes == 65536U);
    garage = lead_config_find_price(&config, "GARAGE");
    CHECK(garage != NULL);
    CHECK(garage->min_per_sqft == 7.40);
    CHECK(garage->max_per_sqft == 8.60);

    CHECK(lead_config_load_json("config/default_config.json", &config, error, sizeof(error)));
    CHECK(config.service_area_city_count == 10U);
    CHECK(config.price_count == 6U);
    CHECK(config.high_urgency_keyword_count == 9U);

    error[0] = '\0';
    CHECK(!lead_config_load_json("fixtures/invalid_config_fractional.json", &config,
                                 error, sizeof(error)));
    CHECK(strstr(error, "max_model_retries") != NULL);

    error[0] = '\0';
    CHECK(!lead_config_load_json("fixtures/invalid_config_price.json", &config,
                                 error, sizeof(error)));
    CHECK(strstr(error, "price") != NULL);

    puts("test_config: PASS");
    return 0;
}
