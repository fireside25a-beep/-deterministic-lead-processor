#ifndef LEAD_PROCESSOR_LEAD_ROUTER_H
#define LEAD_PROCESSOR_LEAD_ROUTER_H

#include "lead.h"

typedef enum {
    ROUTE_SPAM = 0,
    ROUTE_WRONG_SPECIALTY,
    ROUTE_INSUFFICIENT_INFORMATION,
    ROUTE_STANDARD_RESIDENTIAL,
    ROUTE_COMMERCIAL,
    ROUTE_URGENT,
    ROUTE_OUT_OF_AREA,
    ROUTE_MODEL_RETRY,
    ROUTE_MANUAL_REVIEW
} LeadRoute;

LeadRoute lead_route_pre_model(const Lead *lead);
LeadRoute lead_route_final(const Lead *lead, const LeadResult *result);
const char *lead_route_name(LeadRoute route);

#endif
