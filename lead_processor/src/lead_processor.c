#include "lead_processor.h"
#include "json_validator.h"
#include "lead_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void manual_review_result(const Lead *lead, const LeadConfig *config,
                                 const char *last_error, LeadProcessOutcome *outcome) {
    double area = 0.0;
    lead_result_init(&outcome->result);
    outcome->result.fit = false;
    outcome->result.in_area = lead_rules_is_in_service_area(lead, config);
    (void)lead_copy_string(outcome->result.fit_reason, sizeof(outcome->result.fit_reason),
                           "Model response failed validation after retry exhaustion; manual review required.");
    if (lead_rules_extract_area(lead->message, &area)) {
        outcome->result.sqft_present = true;
        outcome->result.sqft = area;
    }
    (void)snprintf(outcome->result.summary, sizeof(outcome->result.summary),
                   "Manual review required for lead %.32s: %.800s", lead->lead_id, lead->message);
    (void)lead_copy_string(outcome->result.next_step, sizeof(outcome->result.next_step), "Manual review");
    outcome->route = ROUTE_MANUAL_REVIEW;
    outcome->manual_review = true;
    if (last_error != NULL) {
        (void)lead_copy_string(outcome->last_error, sizeof(outcome->last_error), last_error);
    }
}

bool lead_process_one(const Lead *lead, const LeadConfig *config,
                      ModelClient *client, LeadProcessOutcome *outcome) {
    unsigned int attempt = 0U;
    unsigned int total_attempts = 0U;
    char error[LEAD_ERROR_MAX] = {0};

    if ((lead == NULL) || (config == NULL) || (client == NULL) || (outcome == NULL)) {
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    total_attempts = config->max_model_retries + 1U;
    for (attempt = 1U; attempt <= total_attempts; ++attempt) {
        char *raw = NULL;
        LeadResult parsed;
        outcome->model_attempts = attempt;
        error[0] = '\0';
        if (!model_client_request(client, lead, &raw, error, sizeof(error))) {
            (void)lead_copy_string(outcome->last_error, sizeof(outcome->last_error), error);
            outcome->retried = attempt < total_attempts;
            free(raw);
            continue;
        }
        if (!json_validate_model_response(raw, &parsed, error, sizeof(error))) {
            (void)lead_copy_string(outcome->last_error, sizeof(outcome->last_error), error);
            outcome->retried = attempt < total_attempts;
            free(raw);
            continue;
        }
        free(raw);
        lead_rules_apply(lead, config, &parsed);
        outcome->result = parsed;
        outcome->route = lead_route_final(lead, &outcome->result);
        outcome->manual_review = false;
        return true;
    }
    manual_review_result(lead, config, outcome->last_error, outcome);
    return true;
}
