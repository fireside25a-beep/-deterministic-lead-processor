#ifndef LEAD_PROCESSOR_JSON_VALIDATOR_H
#define LEAD_PROCESSOR_JSON_VALIDATOR_H

#include "lead.h"

#include <stdbool.h>
#include <stddef.h>

bool json_validate_model_response(const char *json, LeadResult *out,
                                  char *error, size_t error_size);
char *json_serialize_result(const LeadResult *result);
char *json_serialize_envelope(const char *lead_id, const char *route_id,
                              unsigned int attempts, const LeadResult *result);

#endif
