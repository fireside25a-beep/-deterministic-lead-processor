#include "lead_router.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    Lead lead;
    LeadResult result;

    lead_init(&lead);
    lead_result_init(&result);
    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "Our agency guarantees top 3 positions"));
    CHECK(lead_route_pre_model(&lead) == ROUTE_SPAM);
    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "how much?"));
    CHECK(lead_route_pre_model(&lead) == ROUTE_INSUFFICIENT_INFORMATION);
    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "Need a plumber urgently"));
    CHECK(lead_route_pre_model(&lead) == ROUTE_WRONG_SPECIALTY);

    CHECK(lead_copy_string(lead.message, sizeof(lead.message), "Need a warehouse floor coated"));
    result.fit = true;
    result.in_area = true;
    CHECK(lead_copy_string(result.space, sizeof(result.space), "commercial"));
    CHECK(lead_route_final(&lead, &result) == ROUTE_COMMERCIAL);
    CHECK(lead_copy_string(result.urgency, sizeof(result.urgency), "high"));
    CHECK(lead_route_final(&lead, &result) == ROUTE_URGENT);
    result.in_area = false;
    CHECK(lead_route_final(&lead, &result) == ROUTE_OUT_OF_AREA);
    CHECK(strcmp(lead_route_name(ROUTE_MODEL_RETRY), "ROUTE_MODEL_RETRY") == 0);

    puts("test_router: PASS");
    return 0;
}
