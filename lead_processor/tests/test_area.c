#include "lead_rules.h"

#include <math.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#define CLOSE(a,b,t) (fabs((a)-(b)) <= (t))

int main(void) {
    double area = 0.0;

    CHECK(lead_rules_extract_area("Need a 500 sq ft garage", &area));
    CHECK(CLOSE(area, 500.0, 0.001));
    CHECK(lead_rules_extract_area("driveway is about 20 by 30 feet", &area));
    CHECK(CLOSE(area, 600.0, 0.001));
    CHECK(lead_rules_extract_area("garage environ 400 pieds carrés", &area));
    CHECK(CLOSE(area, 400.0, 0.001));
    CHECK(lead_rules_extract_area("sous-sol environ 35 mètres carrés", &area));
    CHECK(CLOSE(area, 376.73686458485, 0.01));
    CHECK(!lead_rules_extract_area("how much?", &area));

    puts("test_area: PASS");
    return 0;
}
