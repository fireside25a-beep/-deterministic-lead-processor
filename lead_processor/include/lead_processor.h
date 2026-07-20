#ifndef LEAD_PROCESSOR_PIPELINE_H
#define LEAD_PROCESSOR_PIPELINE_H

#include "config.h"
#include "lead.h"
#include "lead_router.h"
#include "model_client.h"

#include <stdbool.h>

typedef struct {
    LeadResult result;
    LeadRoute route;
    unsigned int model_attempts;
    bool retried;
    bool manual_review;
    char last_error[LEAD_ERROR_MAX];
} LeadProcessOutcome;

bool lead_process_one(const Lead *lead, const LeadConfig *config,
                      ModelClient *client, LeadProcessOutcome *outcome);

#endif
