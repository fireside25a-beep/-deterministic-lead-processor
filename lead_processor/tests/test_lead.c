#include "lead.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    Lead lead;
    LeadArray array;

    lead_init(&lead);
    CHECK(lead_copy_string(lead.phone, sizeof(lead.phone), "(604) 555-0101"));
    CHECK(lead_copy_string(lead.email, sizeof(lead.email), " Demo@Example.COM "));
    CHECK(lead_copy_string(lead.channel, sizeof(lead.channel), " WhatsApp "));
    lead_normalize(&lead);
    CHECK(strcmp(lead.phone, "+16045550101") == 0);
    CHECK(strcmp(lead.email, "demo@example.com") == 0);
    CHECK(strcmp(lead.channel, "whatsapp") == 0);

    lead_array_init(&array);
    CHECK(lead_array_push(&array, &lead));
    CHECK(array.count == 1U);
    CHECK(strcmp(array.items[0].phone, "+16045550101") == 0);
    lead_array_free(&array);

    puts("test_lead: PASS");
    return 0;
}
