#include "csv_reader.h"
#include "json_validator.h"
#include "lead_processor.h"
#include "cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct {
    size_t exact_rows;
    size_t tolerance_rows;
    size_t mismatch_rows;
    size_t ambiguous_rows;
    size_t impossible_rows;
    size_t unscored_fields;
    size_t retried_rows;
    size_t manual_review_rows;
} Score;

static bool expected_bool(const char *text) {
    return strcmp(text, "true") == 0;
}

static bool compare_row(const Lead *lead, const LeadResult *result, bool *tolerance) {
    double expected_sqft = 0.0;
    double difference = 0.0;

    *tolerance = false;
    if (result->fit != expected_bool(lead->expected_fit)) {
        return false;
    }
    if (strcmp(result->space, lead->expected_space) != 0) {
        return false;
    }
    if (lead->expected_sqft[0] == '\0') {
        if (result->sqft_present) {
            return false;
        }
    } else {
        if (!result->sqft_present) {
            return false;
        }
        expected_sqft = strtod(lead->expected_sqft, NULL);
        difference = fabs(result->sqft - expected_sqft);
        if (difference > 2.0) {
            return false;
        }
        if (difference > 0.01) {
            *tolerance = true;
        }
    }
    if (strcmp(result->urgency, lead->expected_urgency) != 0) {
        return false;
    }
    if (lead->expected_estimate[0] == '\0') {
        if (result->estimate_present) {
            return false;
        }
    } else if (!result->estimate_present || strcmp(result->estimate, lead->expected_estimate) != 0) {
        return false;
    }
    if (strcmp(result->next_step, lead->expected_next_step) != 0) {
        return false;
    }
    if (strcmp(result->draft_reply, lead->expected_draft_reply) != 0) {
        return false;
    }
    return true;
}

int main(void) {
    LeadArray leads;
    LeadConfig config;
    ModelClient client;
    char error[LEAD_ERROR_MAX] = {0};
    Score score = {0};
    size_t i = 0U;

    CHECK(csv_read_leads("workbelt-leads-demo.csv", &leads, error, sizeof(error)));
    CHECK(leads.count == 20U);
    CHECK(lead_config_load_json("config/default_config.json", &config, error, sizeof(error)));
    model_client_init(&client);
    CHECK(model_client_open_mock(&client, "fixtures/mock_model_responses.jsonl", error, sizeof(error)));

    for (i = 0U; i < leads.count; ++i) {
        LeadProcessOutcome outcome;
        char *envelope = NULL;
        cJSON *parsed = NULL;
        const char *end = NULL;
        bool tolerance = false;

        CHECK(lead_process_one(&leads.items[i], &config, &client, &outcome));
        envelope = json_serialize_envelope(leads.items[i].lead_id, lead_route_name(outcome.route),
                                           outcome.model_attempts, &outcome.result);
        CHECK(envelope != NULL);
        parsed = cJSON_ParseWithOpts(envelope, &end, 1);
        CHECK(parsed != NULL && cJSON_IsObject(parsed));
        cJSON_Delete(parsed);
        cJSON_free(envelope);

        if (outcome.retried) {
            ++score.retried_rows;
        }
        if (outcome.manual_review) {
            ++score.manual_review_rows;
        }
        if (strcmp(leads.items[i].lead_id, "12") == 0) {
            CHECK(!outcome.result.fit);
            CHECK(outcome.route == ROUTE_INSUFFICIENT_INFORMATION);
            ++score.ambiguous_rows;
            continue;
        }
        if (compare_row(&leads.items[i], &outcome.result, &tolerance)) {
            if (tolerance) {
                ++score.tolerance_rows;
            } else {
                ++score.exact_rows;
            }
        } else {
            fprintf(stderr, "mismatch lead_id=%s route=%s\n", leads.items[i].lead_id,
                    lead_route_name(outcome.route));
            ++score.mismatch_rows;
        }
    }

    score.impossible_rows = 0U;
    score.unscored_fields = leads.count * 3U; /* fitReason, inArea, summary have no expected columns. */

    CHECK(score.exact_rows == 18U);
    CHECK(score.tolerance_rows == 1U);
    CHECK(score.mismatch_rows == 0U);
    CHECK(score.ambiguous_rows == 1U);
    CHECK(score.impossible_rows == 0U);
    CHECK(score.unscored_fields == 60U);
    CHECK(score.retried_rows == 1U);
    CHECK(score.manual_review_rows == 0U);

    printf("demo comparison: exact=%zu tolerance=%zu mismatch=%zu ambiguous=%zu impossible=%zu unscored_fields=%zu retried=%zu manual_review=%zu\n",
           score.exact_rows, score.tolerance_rows, score.mismatch_rows, score.ambiguous_rows,
           score.impossible_rows, score.unscored_fields, score.retried_rows, score.manual_review_rows);

    model_client_destroy(&client);
    lead_array_free(&leads);
    puts("test_demo_csv: PASS");
    return 0;
}
