#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>

#include <setjmp.h>
#include <cmocka.h>
#include "io/io.h"

void test_simple_call(void** state) {
    (void)state;

    InputBuffer input;
    input_init(&input, "../test.txt");       // bin
    ProcessResult result = process_buffer(input.buf[input.active_buf]);
    printf("pos_count: %zu, space_count: %zu\n", result.pos_count, result.space_count);
    for (size_t i = 0; i < result.pos_count; i++) {
        printf("%u ", result.positions[i]);
    }
    printf("\n");

    free(result.positions);

    input_cleanup(&input);
}

void entry_ib(void** state) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_simple_call)
    };
    cmocka_run_group_tests(tests, NULL, NULL);
}


