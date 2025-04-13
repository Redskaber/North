#include <stdio.h>
#include <stddef.h> 
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>

#include <setjmp.h>
#include <cmocka.h>

#define THREAD_NUM 4
#define OPS_PER_THREAD 10000
#define MACRO_UNUSED(x) (void)(x)

#include "test/api/api_token.h"


// ==============================================================
/// @brief 基础功能测试::池初始化校验
/// @param state 
void test_pool_init(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    TokenBlock* head = test_get_pool_head();
    assert_non_null(head);
    assert_int_equal(head->used, 0);

    TaggerPointer ptr = free_list;
    assert_int_equal(ptr.ptr % CACHE_LINE_SIZE, 0);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ===============================================================
/// @brief 基础功能测试::单线程分配释放顺序性
/// @param state 
void test_alloc_free_sequence(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    Token *t1 = token_alloc(Tk_Ident, (Span){0,0});
    Token *t2 = token_alloc(Tk_Ident, (Span){0,0});
    token_free(t1);     // free_pointer: t1 -> NULL
    token_free(t2);     // free_pointer: t2 -> t1-> NULL
    
    Token *t3 = token_alloc(Tk_Ident, (Span){0,0}); // free_pointer: t1-> NULL
    assert_ptr_equal(t3, t2); // 验证LIFO特性
    Token *t4 = token_alloc(Tk_Ident, (Span){0,0}); // free_pointer: NULL
    assert_ptr_equal(t4, t1);

    assert_ptr_equal(test_get_free_list().ptr, NULL);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ===============================================================
/// @brief 基础功能测试::内存对齐
/// @param state 
void test_memory_alignment(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    Token *t = token_alloc(Tk_Ident, (Span){0,0});
    assert_int_equal((uintptr_t)t % CACHE_LINE_SIZE, 0);
    assert_int_equal((uintptr_t)t->next_free % CACHE_LINE_SIZE, 0);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ===============================================================
/// @brief 边界条件测试::池耗尽时自动扩展
/// @param state 
void test_pool_expansion(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    size_t initial_total = test_get_total_allocated();
    for (int i=0; i<TOKEN_POOL_BLOCK+1; i++) {
        token_alloc(Tk_Ident, (Span){0,0});
    }
    assert_int_equal(test_get_total_allocated(), initial_total + TOKEN_POOL_BLOCK +1);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ===============================================================
/// @brief 边界条件测试::双重释放检测
/// @param state
void test_double_free(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    Token *t = token_alloc(Tk_Ident, (Span){0,0});
    assert_int_equal(t->type, Tk_Ident);
    token_free(t);
    assert_int_not_equal(t->type, Tk_Ident);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ===============================================================
/// @brief 并发测试::多线程竞争测试
/// @param state 
struct thread_args {
    int id;
    Token **ptrs;
};

void* thread_alloc_free(void *arg) {
    struct thread_args *args = arg;
    for (int i=0; i<OPS_PER_THREAD; i++) {
        args->ptrs[i] = token_alloc(Tk_Ident, (Span){0,0});
        token_free(args->ptrs[i]);
    }
    return NULL;
}

void test_concurrent_ops(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    pthread_t threads[THREAD_NUM];
    struct thread_args args[THREAD_NUM];
    Token *ptrs[THREAD_NUM][OPS_PER_THREAD];

    // 创建线程
    for (int i=0; i<THREAD_NUM; i++) {
        args[i].id = i;
        args[i].ptrs = ptrs[i];
        pthread_create(&threads[i], NULL, thread_alloc_free, &args[i]);
    }
    
    // 等待线程完成
    for (int i=0; i<THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }

    // 验证无内存泄漏
    assert(test_get_total_allocated() && "Memory leak detected!");
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
//  ===============================================================
/// @brief 异常场景测试::无效指针释放检测
/// @param state 
void test_invalid_free(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    Token invalid_token;
    token_free(&invalid_token); // 未对齐的指针
    assert_int_equal(&invalid_token, test_get_free_list().ptr);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ================================================================
/// @brief 边界条件测试::跨Block分配检测
/// @param state 
void test_cross_block_allocation(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    // 分配完第一个Block
    Token *first_block[TOKEN_POOL_BLOCK];
    for (int i=0; i<TOKEN_POOL_BLOCK; i++) {
        first_block[i] = token_alloc(Tk_Ident, (Span){0,0});
    }
    
    // 分配应触发新Block
    Token *t = token_alloc(Tk_Ident, (Span){0,0});
    assert_non_null(t);
    assert_ptr_not_equal(t, first_block[0]);
    token_pool_cleanup();
    assert_int_equal(test_get_total_allocated(), 0);
}
// ================================================================
void test_order_base(void **state) {
    MACRO_UNUSED(state);
    token_pool_init(TOKEN_POOL_BLOCK);
    Token* t1 = token_alloc(Tk_And, (Span){0, 0});
    Token* t2 = token_alloc(Tk_Or, (Span){0, 1});
    Token* t3 = token_alloc(Tk_And, (Span){0, 2});
    Token* t4 = token_alloc(Tk_Or, (Span){0, 3});
    Token* t5 = token_alloc(Tk_And, (Span){0, 4});

    printf("t1: %p, t2: %p, t3: %p, t4: %p, t5: %p\n", t1, t2, t3, t4, t5);
    printf("t1->type: %d, t2->type: %d, t3->type: %d, t4->type: %d, t5->type: %d\n", 
        t1->type, t2->type, t3->type, t4->type, t5->type);
    
    token_free(t1);
    token_free(t2);
    token_free(t3);
    
    Token* t6 = token_alloc(Tk_Or, (Span){0, 5});
    Token* t7 = token_alloc(Tk_And, (Span){0, 6});

    printf("t6: %p, t7: %p\n", t6, t7);
    printf("t6->type: %d, t7->type: %d\n", t6->type, t7->type);

    token_free(t4);
    token_free(t5);
    token_free(t6);
    token_free(t7);
    token_pool_cleanup();
    printf("total_allocated: %lu\n", total_allocated);
}

int test_setup(void **state) {
    token_pool_init(0);
    *state = NULL;
    return 0;
}

void entry_token(void** state) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_pool_init, test_setup),
        cmocka_unit_test_setup(test_alloc_free_sequence, test_setup),
        cmocka_unit_test_setup(test_memory_alignment, test_setup),
        cmocka_unit_test_setup(test_pool_expansion, test_setup),
        cmocka_unit_test_setup(test_double_free, test_setup),
        cmocka_unit_test_setup(test_concurrent_ops, test_setup),
        cmocka_unit_test_setup(test_invalid_free, test_setup),
        cmocka_unit_test_setup(test_cross_block_allocation, test_setup),
        cmocka_unit_test_setup(test_order_base, test_setup),
    };
    cmocka_run_group_tests(tests, NULL, NULL);
}
