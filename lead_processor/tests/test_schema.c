#include "json_validator.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static const char *valid_json =
    "{\"fit\":true,\"fitReason\":\"Floor coating request\",\"space\":\"garage\","
    "\"sqft\":600,\"inArea\":true,\"urgency\":\"medium\",\"summary\":\"Garage coating\","
    "\"estimate\":null,\"nextStep\":\"Book assessment\",\"draftReply\":\"Hello\"}";

int main(void) {
    LeadResult result;
    char error[LEAD_ERROR_MAX] = {0};
    char *serialized = NULL;

    CHECK(json_validate_model_response(valid_json, &result, error, sizeof(error)));
    CHECK(result.fit);
    CHECK(result.sqft_present && result.sqft == 600.0);
    CHECK(!result.estimate_present);

    error[0] = '\0';
    CHECK(!json_validate_model_response("{}", &result, error, sizeof(error)));
    CHECK(strstr(error, "missing required") != NULL);

    error[0] = '\0';
    CHECK(!json_validate_model_response(
        "{\"fit\":true,\"fitReason\":\"x\",\"space\":\"garage\",\"sqft\":null,"
        "\"inArea\":true,\"urgency\":\"critical\",\"summary\":\"x\",\"estimate\":null,"
        "\"nextStep\":\"x\",\"draftReply\":\"x\"}", &result, error, sizeof(error)));
    CHECK(strstr(error, "urgency") != NULL);

    error[0] = '\0';
    CHECK(!json_validate_model_response("[]", &result, error, sizeof(error)));
    CHECK(!json_validate_model_response("{} trailing", &result, error, sizeof(error)));

    CHECK(json_validate_model_response(valid_json, &result, error, sizeof(error)));
    serialized = json_serialize_result(&result);
    CHECK(serialized != NULL);
    CHECK(json_validate_model_response(serialized, &result, error, sizeof(error)));
    cJSON_free(serialized);

    serialized = json_serialize_envelope("42", "ROUTE_STANDARD_RESIDENTIAL", 1U, &result);
    CHECK(serialized != NULL);
    CHECK(strstr(serialized, "ROUTE_STANDARD_RESIDENTIAL") != NULL);
    cJSON_free(serialized);

    puts("test_schema: PASS");
    return 0;
}
