#include "lead_rules.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    LeadConfig config;
    LeadResult result;
    char estimate[LEAD_ESTIMATE_MAX];
    char error[LEAD_ERROR_MAX] = {0};

    CHECK(lead_config_load_json("config/default_config.json", &config, error, sizeof(error)));
    lead_result_init(&result);
    result.fit = true;
    result.in_area = true;
    result.sqft_present = true;
    result.sqft = 500.0;
    CHECK(lead_copy_string(result.space, sizeof(result.space), "garage"));
    CHECK(lead_rules_calculate_estimate(&result, &config, estimate, sizeof(estimate)));
    CHECK(strcmp(estimate, "$3700–$4300") == 0);

    result.sqft = 600.0;
    CHECK(lead_copy_string(result.space, sizeof(result.space), "driveway"));
    CHECK(lead_rules_calculate_estimate(&result, &config, estimate, sizeof(estimate)));
    CHECK(strcmp(estimate, "$5500–$6500") == 0);

    result.in_area = false;
    CHECK(!lead_rules_calculate_estimate(&result, &config, estimate, sizeof(estimate)));

    puts("test_pricing: PASS");
    return 0;
}
