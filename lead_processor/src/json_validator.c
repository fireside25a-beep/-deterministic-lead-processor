#include "json_validator.h"
#include "cJSON.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if ((error != NULL) && (error_size > 0U)) {
        (void)lead_copy_string(error, error_size, message);
    }
}

static cJSON *required_item(cJSON *root, const char *name, char *error, size_t error_size) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    char message[LEAD_ERROR_MAX];
    if (item == NULL) {
        (void)snprintf(message, sizeof(message), "missing required JSON key: %s", name);
        set_error(error, error_size, message);
    }
    return item;
}

static bool copy_json_string(cJSON *item, const char *name, char *dst, size_t dst_size,
                             char *error, size_t error_size) {
    char message[LEAD_ERROR_MAX];
    if (!cJSON_IsString(item) || (item->valuestring == NULL)) {
        (void)snprintf(message, sizeof(message), "%s must be a string", name);
        set_error(error, error_size, message);
        return false;
    }
    if (!lead_copy_string(dst, dst_size, item->valuestring)) {
        (void)snprintf(message, sizeof(message), "%s exceeds the %zu-byte field limit", name, dst_size - 1U);
        set_error(error, error_size, message);
        return false;
    }
    return true;
}

static bool urgency_valid(const char *urgency) {
    return (strcmp(urgency, "low") == 0) || (strcmp(urgency, "medium") == 0) ||
           (strcmp(urgency, "high") == 0);
}

bool json_validate_model_response(const char *json, LeadResult *out,
                                  char *error, size_t error_size) {
    const char *parse_end = NULL;
    cJSON *root = NULL;
    cJSON *fit = NULL;
    cJSON *fit_reason = NULL;
    cJSON *space = NULL;
    cJSON *sqft = NULL;
    cJSON *in_area = NULL;
    cJSON *urgency = NULL;
    cJSON *summary = NULL;
    cJSON *estimate = NULL;
    cJSON *next_step = NULL;
    cJSON *draft_reply = NULL;
    LeadResult parsed;

    if ((json == NULL) || (out == NULL)) {
        set_error(error, error_size, "model JSON or output pointer is null");
        return false;
    }
    lead_result_init(&parsed);
    root = cJSON_ParseWithOpts(json, &parse_end, 1);
    if (root == NULL) {
        set_error(error, error_size, "model response is malformed JSON or contains trailing text");
        return false;
    }
    if (!cJSON_IsObject(root)) {
        set_error(error, error_size, "model response must be one top-level JSON object");
        cJSON_Delete(root);
        return false;
    }

    fit = required_item(root, "fit", error, error_size);
    fit_reason = required_item(root, "fitReason", error, error_size);
    space = required_item(root, "space", error, error_size);
    sqft = required_item(root, "sqft", error, error_size);
    in_area = required_item(root, "inArea", error, error_size);
    urgency = required_item(root, "urgency", error, error_size);
    summary = required_item(root, "summary", error, error_size);
    estimate = required_item(root, "estimate", error, error_size);
    next_step = required_item(root, "nextStep", error, error_size);
    draft_reply = required_item(root, "draftReply", error, error_size);
    if ((fit == NULL) || (fit_reason == NULL) || (space == NULL) || (sqft == NULL) ||
        (in_area == NULL) || (urgency == NULL) || (summary == NULL) || (estimate == NULL) ||
        (next_step == NULL) || (draft_reply == NULL)) {
        cJSON_Delete(root);
        return false;
    }

    if (!cJSON_IsBool(fit)) {
        set_error(error, error_size, "fit must be boolean");
        cJSON_Delete(root);
        return false;
    }
    parsed.fit = cJSON_IsTrue(fit);
    if (!copy_json_string(fit_reason, "fitReason", parsed.fit_reason, sizeof(parsed.fit_reason), error, error_size) ||
        !copy_json_string(space, "space", parsed.space, sizeof(parsed.space), error, error_size)) {
        cJSON_Delete(root);
        return false;
    }
    if (cJSON_IsNull(sqft)) {
        parsed.sqft_present = false;
    } else if (cJSON_IsNumber(sqft) && isfinite(sqft->valuedouble) &&
               (sqft->valuedouble > 0.0) && (sqft->valuedouble <= 10000000.0)) {
        parsed.sqft_present = true;
        parsed.sqft = sqft->valuedouble;
    } else {
        set_error(error, error_size, "sqft must be null or a positive bounded number");
        cJSON_Delete(root);
        return false;
    }
    if (!cJSON_IsBool(in_area)) {
        set_error(error, error_size, "inArea must be boolean");
        cJSON_Delete(root);
        return false;
    }
    parsed.in_area = cJSON_IsTrue(in_area);
    if (!copy_json_string(urgency, "urgency", parsed.urgency, sizeof(parsed.urgency), error, error_size) ||
        !urgency_valid(parsed.urgency)) {
        if (error != NULL && error[0] == '\0') {
            set_error(error, error_size, "urgency must be low, medium, or high");
        } else if (urgency_valid(parsed.urgency) == false) {
            set_error(error, error_size, "urgency must be low, medium, or high");
        }
        cJSON_Delete(root);
        return false;
    }
    if (!copy_json_string(summary, "summary", parsed.summary, sizeof(parsed.summary), error, error_size)) {
        cJSON_Delete(root);
        return false;
    }
    if (cJSON_IsNull(estimate)) {
        parsed.estimate_present = false;
    } else if (cJSON_IsString(estimate) && (estimate->valuestring != NULL)) {
        if (!lead_copy_string(parsed.estimate, sizeof(parsed.estimate), estimate->valuestring)) {
            set_error(error, error_size, "estimate exceeds its field limit");
            cJSON_Delete(root);
            return false;
        }
        parsed.estimate_present = true;
    } else {
        set_error(error, error_size, "estimate must be a string or null");
        cJSON_Delete(root);
        return false;
    }
    if (!copy_json_string(next_step, "nextStep", parsed.next_step, sizeof(parsed.next_step), error, error_size) ||
        !copy_json_string(draft_reply, "draftReply", parsed.draft_reply, sizeof(parsed.draft_reply), error, error_size)) {
        cJSON_Delete(root);
        return false;
    }

    *out = parsed;
    cJSON_Delete(root);
    return true;
}

static cJSON *result_object(const LeadResult *result) {
    cJSON *object = NULL;
    if (result == NULL) {
        return NULL;
    }
    object = cJSON_CreateObject();
    if (object == NULL) {
        return NULL;
    }
    if ((cJSON_AddBoolToObject(object, "fit", result->fit) == NULL) ||
        (cJSON_AddStringToObject(object, "fitReason", result->fit_reason) == NULL) ||
        (cJSON_AddStringToObject(object, "space", result->space) == NULL)) {
        cJSON_Delete(object);
        return NULL;
    }
    if (result->sqft_present) {
        if (cJSON_AddNumberToObject(object, "sqft", result->sqft) == NULL) {
            cJSON_Delete(object);
            return NULL;
        }
    } else if (cJSON_AddNullToObject(object, "sqft") == NULL) {
        cJSON_Delete(object);
        return NULL;
    }
    if ((cJSON_AddBoolToObject(object, "inArea", result->in_area) == NULL) ||
        (cJSON_AddStringToObject(object, "urgency", result->urgency) == NULL) ||
        (cJSON_AddStringToObject(object, "summary", result->summary) == NULL)) {
        cJSON_Delete(object);
        return NULL;
    }
    if (result->estimate_present) {
        if (cJSON_AddStringToObject(object, "estimate", result->estimate) == NULL) {
            cJSON_Delete(object);
            return NULL;
        }
    } else if (cJSON_AddNullToObject(object, "estimate") == NULL) {
        cJSON_Delete(object);
        return NULL;
    }
    if ((cJSON_AddStringToObject(object, "nextStep", result->next_step) == NULL) ||
        (cJSON_AddStringToObject(object, "draftReply", result->draft_reply) == NULL)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

char *json_serialize_result(const LeadResult *result) {
    cJSON *object = result_object(result);
    char *json = NULL;
    if (object == NULL) {
        return NULL;
    }
    json = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    return json;
}

char *json_serialize_envelope(const char *lead_id, const char *route_id,
                              unsigned int attempts, const LeadResult *result) {
    cJSON *root = NULL;
    cJSON *object = NULL;
    char *json = NULL;

    if ((lead_id == NULL) || (route_id == NULL) || (result == NULL)) {
        return NULL;
    }
    root = cJSON_CreateObject();
    object = result_object(result);
    if ((root == NULL) || (object == NULL) ||
        (cJSON_AddStringToObject(root, "leadId", lead_id) == NULL) ||
        (cJSON_AddStringToObject(root, "routeId", route_id) == NULL) ||
        (cJSON_AddNumberToObject(root, "modelAttempts", (double)attempts) == NULL) ||
        !cJSON_AddItemToObject(root, "result", object)) {
        cJSON_Delete(object);
        cJSON_Delete(root);
        return NULL;
    }
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
