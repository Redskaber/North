#include <stdarg.h>
#include <stddef.h>

#include <setjmp.h>
#include <cmocka.h>

#include "test_north.h"


int main(void) {
    const struct CMUnitTest sub_tests[] = {
        cmocka_unit_test(entry_ib),
        cmocka_unit_test(entry_token),
        cmocka_unit_test(entry_generic_pool),
    };
    return cmocka_run_group_tests(sub_tests, NULL, NULL);
}
