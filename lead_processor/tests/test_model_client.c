#include "model_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    ModelClient client;
    LeadConfig config;
    Lead lead;
    char error[LEAD_ERROR_MAX] = {0};
    char *raw = NULL;

    model_client_init(&client);
    lead_config_defaults(&config);
    lead_init(&lead);
    CHECK(lead_copy_string(lead.lead_id, sizeof(lead.lead_id), "retry"));
    CHECK(model_client_open_mock(&client, "fixtures/mock_retry_fallback.jsonl", error, sizeof(error)));
    CHECK(model_client_request(&client, &lead, &raw, error, sizeof(error)));
    CHECK(strcmp(raw, "bad json one") == 0);
    free(raw);
    raw = NULL;
    CHECK(model_client_request(&client, &lead, &raw, error, sizeof(error)));
    CHECK(strstr(raw, "fitReason") != NULL);
    free(raw);
    model_client_destroy(&client);

    unsetenv("LEAD_API_KEY");
    CHECK(!model_client_open_live_from_env(&client, &config, error, sizeof(error)));
    CHECK(strstr(error, "LEAD_API_KEY") != NULL);

    CHECK(setenv("LEAD_API_KEY", "test-only-key", 1) == 0);
    CHECK(setenv("LEAD_API_BASE_URL", "http://127.0.0.1:1/v1", 1) == 0);
    CHECK(setenv("LEAD_API_MODEL", "test-model", 1) == 0);
    config.request_timeout_seconds = 1L;
    CHECK(model_client_open_live_from_env(&client, &config, error, sizeof(error)));
    CHECK(!model_client_request(&client, &lead, &raw, error, sizeof(error)));
    CHECK(strstr(error, "HTTP request failed") != NULL);
    model_client_destroy(&client);
    unsetenv("LEAD_API_KEY");
    unsetenv("LEAD_API_BASE_URL");
    unsetenv("LEAD_API_MODEL");

    puts("test_model_client: PASS");
    return 0;
}
