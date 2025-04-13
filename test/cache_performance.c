#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "token.h"

void test_cache_performance() {
    const int N = 1024*1024;
    // 初始化内存池（必须放在最前面）
    token_pool_init(N * 2);  // 预分配2倍容量
    
    Token** tokens = malloc(N * sizeof(Token*));
    if (!tokens) {
        perror("malloc tokens failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < N; ++i) {
        tokens[i] = token_alloc(Tk_Ident, (Span){0,0});
        if (!tokens[i]) {
            fprintf(stderr, "[FATAL] Token alloc failed at %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // 并行操作
    // #pragma omp parallel for
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < 1000; ++j) {
            // 原子操作避免竞态条件
            __atomic_fetch_add(&tokens[i]->span.start, 1, __ATOMIC_RELAXED);
        }
    }
    
    for (int i = 0; i < N; ++i) {
        if (tokens[i]) {
            token_free(tokens[i]);
        }
    }
    free(tokens);
}

int run_cache_performance() {
    printf("Testing cache performance...\n");
    __clock_t start = clock();
    test_cache_performance();
    printf("Cache performance test took %.2f seconds.\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    printf("Cache performance test passed.\n");
    return 0;
}
