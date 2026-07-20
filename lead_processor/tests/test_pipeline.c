#include "lead_processor.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    LeadConfig config;
    ModelClient client;
    Lead lead;
    LeadProcessOutcome outcome;
    char error[LEAD_ERROR_MAX] = {0};

    lead_config_defaults(&config);
    config.max_model_retries = 2U;
    model_client_init(&client);
    CHECK(model_client_open_mock(&client, "fixtures/mock_retry_fallback.jsonl", error, sizeof(error)));

    lead_init(&lead);
    CHECK(lead_copy_string(lead.lead_id, sizeof(lead.lead_id), "retry"));
    CHECK(lead_copy_string(lead.city, sizeof(lead.city), "Vancouver"));
    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "Need a 500 sq ft garage coated"));
    CHECK(lead_process_one(&lead, &config, &client, &outcome));
    CHECK(!outcome.manual_review);
    CHECK(outcome.retried);
    CHECK(outcome.model_attempts == 2U);
    CHECK(outcome.route == ROUTE_STANDARD_RESIDENTIAL);
    CHECK(outcome.result.estimate_present);
    CHECK(strcmp(outcome.result.estimate, "$3700–$4300") == 0);
    model_client_destroy(&client);

    model_client_init(&client);
    CHECK(model_client_open_mock(&client, "fixtures/mock_retry_fallback.jsonl", error, sizeof(error)));
    lead_init(&lead);
    CHECK(lead_copy_string(lead.lead_id, sizeof(lead.lead_id), "fallback"));
    CHECK(lead_copy_string(lead.city, sizeof(lead.city), "Vancouver"));
    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "Need a 20 by 30 feet garage coating"));
    CHECK(lead_process_one(&lead, &config, &client, &outcome));
    CHECK(outcome.manual_review);
    CHECK(outcome.model_attempts == 3U);
    CHECK(outcome.route == ROUTE_MANUAL_REVIEW);
    CHECK(outcome.result.sqft_present && outcome.result.sqft == 600.0);
    CHECK(strcmp(outcome.result.next_step, "Manual review") == 0);
    model_client_destroy(&client);

    puts("test_pipeline: PASS");
    return 0;
}
