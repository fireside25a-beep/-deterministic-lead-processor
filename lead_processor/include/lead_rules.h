#ifndef LEAD_PROCESSOR_LEAD_RULES_H
#define LEAD_PROCESSOR_LEAD_RULES_H

#include "config.h"
#include "lead.h"

#include <stdbool.h>
#include <stddef.h>

bool lead_rules_extract_area(const char *message, double *sqft_out);
bool lead_rules_is_in_service_area(const Lead *lead, const LeadConfig *config);
bool lead_rules_calculate_estimate(const LeadResult *result, const LeadConfig *config,
                                   char *estimate, size_t estimate_size);
void lead_rules_apply(const Lead *lead, const LeadConfig *config, LeadResult *result);
bool lead_rules_is_spam(const char *message);
bool lead_rules_is_insufficient(const char *message);
bool lead_rules_is_wrong_specialty(const char *message);

#endif
