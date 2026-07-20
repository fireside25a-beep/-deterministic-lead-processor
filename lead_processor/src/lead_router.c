#include "lead_router.h"
#include "lead_rules.h"

#include <string.h>

LeadRoute lead_route_pre_model(const Lead *lead) {
    if (lead == NULL) {
        return ROUTE_MANUAL_REVIEW;
    }
    if (lead_rules_is_spam(lead->message)) {
        return ROUTE_SPAM;
    }
    if (lead_rules_is_insufficient(lead->message)) {
        return ROUTE_INSUFFICIENT_INFORMATION;
    }
    if (lead_rules_is_wrong_specialty(lead->message)) {
        return ROUTE_WRONG_SPECIALTY;
    }
    return ROUTE_STANDARD_RESIDENTIAL;
}

LeadRoute lead_route_final(const Lead *lead, const LeadResult *result) {
    LeadRoute preliminary = lead_route_pre_model(lead);
    if ((preliminary == ROUTE_SPAM) || (preliminary == ROUTE_INSUFFICIENT_INFORMATION) ||
        (preliminary == ROUTE_WRONG_SPECIALTY)) {
        return preliminary;
    }
    if (result == NULL) {
        return ROUTE_MANUAL_REVIEW;
    }
    if (!result->fit) {
        return ROUTE_WRONG_SPECIALTY;
    }
    if (!result->in_area) {
        return ROUTE_OUT_OF_AREA;
    }
    if (strcmp(result->urgency, "high") == 0) {
        return ROUTE_URGENT;
    }
    if (strcmp(result->space, "commercial") == 0) {
        return ROUTE_COMMERCIAL;
    }
    return ROUTE_STANDARD_RESIDENTIAL;
}

const char *lead_route_name(LeadRoute route) {
    switch (route) {
        case ROUTE_SPAM: return "ROUTE_SPAM";
        case ROUTE_WRONG_SPECIALTY: return "ROUTE_WRONG_SPECIALTY";
        case ROUTE_INSUFFICIENT_INFORMATION: return "ROUTE_INSUFFICIENT_INFORMATION";
        case ROUTE_STANDARD_RESIDENTIAL: return "ROUTE_STANDARD_RESIDENTIAL";
        case ROUTE_COMMERCIAL: return "ROUTE_COMMERCIAL";
        case ROUTE_URGENT: return "ROUTE_URGENT";
        case ROUTE_OUT_OF_AREA: return "ROUTE_OUT_OF_AREA";
        case ROUTE_MODEL_RETRY: return "ROUTE_MODEL_RETRY";
        case ROUTE_MANUAL_REVIEW: return "ROUTE_MANUAL_REVIEW";
        default: return "ROUTE_MANUAL_REVIEW";
    }
}
