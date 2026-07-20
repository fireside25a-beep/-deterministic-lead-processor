#include "csv_reader.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    LeadArray leads;
    char error[LEAD_ERROR_MAX] = {0};

    CHECK(csv_read_leads("fixtures/csv_edge_cases.csv", &leads, error, sizeof(error)));
    CHECK(leads.count == 2U);
    CHECK(strcmp(leads.items[0].customer_name, "Zoë Martin") == 0);
    CHECK(strcmp(leads.items[0].message, "Need a garage; about 20 by 30 feet") == 0);
    CHECK(strcmp(leads.items[0].phone, "+16045550199") == 0);
    CHECK(strcmp(leads.items[1].customer_name, "O\"Connor, Pat") == 0);
    CHECK(strcmp(leads.items[1].email, "pat@example.com") == 0);
    CHECK(strstr(leads.items[1].message, "mètres carrés") != NULL);
    lead_array_free(&leads);

    CHECK(csv_read_leads("workbelt-leads-demo.csv", &leads, error, sizeof(error)));
    CHECK(leads.count == 20U);
    CHECK(strcmp(leads.items[0].lead_id, "1") == 0);
    CHECK(strcmp(leads.items[19].lead_id, "20") == 0);
    CHECK(strcmp(leads.items[5].customer_name, "Marie Tremblay") == 0);
    CHECK(strstr(leads.items[12].message, "mètres carrés") != NULL);
    lead_array_free(&leads);

    puts("test_csv: PASS");
    return 0;
}
