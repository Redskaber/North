#include <stdio.h> 
#include "io.h"

bool test_io() {
    InputBuffer input;
    input_init(&input, "../../test.txt");       // bin
    ProcessResult result = process_buffer(input.buf[input.active_buf]);
    printf("pos_count: %zu, space_count: %zu\n", result.pos_count, result.space_count);
    for (size_t i = 0; i < result.pos_count; i++) {
        printf("%u ", result.positions[i]);
    }
    printf("\n");

    free(result.positions);

    input_cleanup(&input);
    return true;    
}

int run_io() {
    return test_io();
}
