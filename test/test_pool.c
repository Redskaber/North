
#include <stdio.h>
#include <stddef.h> 
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#else
#ifndef __USE_POSIX199309
#define __USE_POSIX199309
#endif
#include <time.h>
#endif

#include <assert.h>
#include <pthread.h>

#include <setjmp.h>
#include <cmocka.h>


#include "api/api_pool.h"


#define TEST_COUNT 10000000UL


// =======================================================================================
/**
 * @brief 获取高精度时间戳（单位：秒）
 * 
 * 本函数提供跨平台的高精度时间测量功能，精度可达纳秒级别。
 * Windows平台使用性能计数器实现，Unix-like系统使用clock_gettime实现。
 * 
 * @return double 返回自某个固定点以来的时间（通常系统启动时间），
 *                单位秒，具有单调递增特性（不受系统时间调整影响）
 */
__attribute__((unused)) 
double get_high_res_time() {
#ifdef _WIN32
    /* Windows高性能计时器实现 */
    LARGE_INTEGER freq, count;
    // 获取计数器频率（每秒计数次数）
    QueryPerformanceFrequency(&freq);
    // 获取当前计数器值
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    /* UNIX系统单调时钟实现 */
    struct timespec ts;
    // 获取不受系统时钟调整影响的单调时间
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}
// =======================================================================================





// =======================================================================================
// 基础功能测试
static void test_basic_operations(void** state) {
    Pool* pool = pool_create(sizeof(int), 100);
    assert_non_null(pool);

    // 测试初始状态
    assert_int_equal(get_alloc_cnt(pool), 0);
    assert_int_equal(get_free_cnt(pool), 0);

    // 单次分配释放
    int* p1 = pool_alloc(pool);
    assert_non_null(p1);
    *p1 = 0xDEADBEEF;
    pool_free(pool, p1);
    
    // 验证计数
    assert_int_equal(get_alloc_cnt(pool), 1);
    assert_int_equal(get_free_cnt(pool), 1);
    
    pool_destroy(&pool);
}
// =======================================================================================


// =======================================================================================
// 边界条件测试
static void test_edge_cases(void** state) {
    // 测试容量为0
    Pool* invalid_pool = pool_create(16, 0);
    assert_null(invalid_pool);
    
    // 测试对象大小为0 error
    // invalid_pool = pool_create(0, 100);
    // assert_null(invalid_pool);

    // 创建有效池
    Pool* pool = pool_create(64, 10);
    assert_non_null(pool);

    // 分配超过容量
    void* objs[20];
    size_t count = pool_alloc_batch(pool, objs, 20);
    assert_int_equal(count, 10);
    
    // 测试池耗尽
    // assert_null(pool_alloc(pool));
    
    // only free allocated objects -> destroy pool
    pool_free_batch(pool, objs, count);
    pool_destroy(&pool);
}
// =======================================================================================


// =======================================================================================
// 内存对齐验证
static void test_memory_alignment(void** state) {
    const size_t test_sizes[] = {1, 16, 32, 64, 128};
    for (size_t i = 0; i < sizeof(test_sizes)/sizeof(test_sizes[0]); i++) {
        Pool* pool = pool_create(test_sizes[i], 10);
        assert_non_null(pool);

        for (int j = 0; j < 10; j++) {
            void* ptr = pool_alloc(pool);
            assert_non_null(ptr);
            // 验证地址对齐
            assert_int_equal((uintptr_t)ptr % 16, 0);
            pool_free(pool, ptr);
        }
        pool_destroy(&pool);
    }
}
// =======================================================================================


// =======================================================================================
// 测试用例：基本功能
static void test_base_run(void**state) {
    (void)state;

    Pool* pool = pool_create(sizeof(int), 1024);
    assert_non_null(pool);
    pool_free(pool, pool_alloc(pool));
    assert_int_equal(get_alloc_cnt(pool), 1);
    assert_int_equal(get_free_cnt(pool), 1);
    
    void* objs[1024];
    size_t count = pool_alloc_batch(pool, objs, 1024);
    assert_int_equal(count, 1024);
    pool_free_batch(pool, objs, 1024);
    
    // 原因：回收的内存先在本地线程缓存中缓存，如果本地线程缓存已经满，则回退到内存池
    // 这里是将本地线程缓存中的内存回收到内存池中
    pool_flush_cache(pool);

    printf("alloc: %zu, free: %zu\n", get_alloc_cnt(pool), get_free_cnt(pool));

    assert_int_equal(get_alloc_cnt(pool), 1025);
    assert_int_equal(get_free_cnt(pool), 1025);

    show_pool_info(pool);
    pool_destroy(&pool);
    assert_null(pool);
}
// =======================================================================================


// =======================================================================================
void benchmark_single_thread() {
    Pool* pool = pool_create(sizeof(int), TEST_COUNT+1);

    for(size_t i = 0; i < 1000; ++i) {
        int* p = pool_alloc(pool);
        *p = 0xDEADBEEF;
        pool_free(pool, p);
    }

    double start = get_high_res_time();
    for (size_t i = 0; i < TEST_COUNT; ++i) {
        int* p = pool_alloc(pool);
        *p = (int)i;
        pool_free(pool, p);
    }
    double duration = get_high_res_time() - start;
    if(duration < 1e-9) {
        printf("WARNING: benchmark_single_thread() duration is too short, please try again.\n");
    } else {
        printf("Single thread: %.2f ops/sec\n", TEST_COUNT/duration);
    }
    pool_destroy(&pool);
    assert(pool == NULL);
}
// =======================================================================================


// =======================================================================================
void benchmark_batch_ops() {
    Pool* pool = pool_create(sizeof(int), 1000000);
    void* objs[100];

    for (int i = 0; i < 1000; ++i) {
        size_t count = pool_alloc_batch(pool, objs, 100);
        pool_free_batch(pool, objs, count);
    }

    size_t total_alloc = 0;
    const size_t iterations = TEST_COUNT / 100;    // 100000次迭代

    double start = get_high_res_time();
    for (size_t i = 0; i < iterations; ++i) {
        size_t count = pool_alloc_batch(pool, objs, 100);
        assert(count == 100);                      // assert分配数量
        pool_free_batch(pool, objs, count);
        total_alloc += count;
    }
    double duration = get_high_res_time() - start;
    assert(total_alloc == iterations * 100);

    if (duration < 1e-9) {                          // 1ns
        printf("Batch(100) ops: [measurement error] duration: %f\n", duration);
    } else {
        double ops_per_sec = (double)(iterations * 100) / duration;     // 每次迭代100个操作
        printf("Batch(100) ops: %.2f Mops/sec\n", ops_per_sec / 1e6);
    }

    pool_destroy(&pool);
}
// =======================================================================================


// =======================================================================================
void test_memory_leak() {
    #ifdef __SANITIZE_ADDRESS__
    printf("Running with address sanitizer\n");
    #else
    printf("Compile with -fsanitize=address for leak check\n");
    #endif

    for (int i = 0; i < 1000; ++i) {
        Pool* pool = pool_create(64, 1000);
        void* obj = pool_alloc(pool);
        pool_free(pool, obj);
        pool_destroy(&pool);
        assert(pool == NULL);
    }
    printf("Memory leak test completed\n");
}
// =======================================================================================


// =======================================================================================
void test_stress_memory() {
    const int ITERATIONS = 1000000;
    for (int i = 0; i < ITERATIONS; ++i) {
        Pool* pool = pool_create(64, 1024);
        void* objs[1024];

        // 分配并保留对象不释放
        size_t count = pool_alloc_batch(pool, objs, 1024);
        assert(count == 1024);

        // 仅清理池不释放对象(内存泄露错误)
        // pool_destroy(&pool);

        pool_free_batch(pool, objs, 1024);
        pool_destroy(&pool);
        assert(pool == NULL);
    }
}
// =======================================================================================


// =======================================================================================
// 测试用例：模拟 8 线程并发分配/释放
#define THREAD_NUM 8
#define OPS_PER_THREAD 100000

static void* thread_func(void* arg) {
    Pool* pool = (Pool*)arg;
    for (int i=0; i<OPS_PER_THREAD; i++) {
        void* obj = pool_alloc(pool);
        if (obj) {
            pool_free(pool, obj);
        }
    }
    return NULL;
}

static void test_concurrent_alloc_free(void** state) {
    Pool* pool = pool_create(sizeof(int), 1024);
    pthread_t threads[THREAD_NUM];
    for (int i=0; i<THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, thread_func, pool);
    }
    for (int i=0; i<THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }
    assert_int_equal(get_alloc_cnt(pool), get_free_cnt(pool));
    pool_destroy(&pool);
    assert_null(pool);
}
// =======================================================================================
// 多线程性能基准测试
#define CONCURRENT_THREADS 8
#define CONCURRENT_OPS_PER_THREAD 1000000

static void* concurrent_bench_thread(void* arg) {
    Pool* pool = (Pool*)arg;
    for (size_t i = 0; i < CONCURRENT_OPS_PER_THREAD; ++i) {
        int* p = pool_alloc(pool);
        if(p) {
            *p = (int)i;
            pool_free(pool, p);
        }
    }
    return NULL;
}

void test_benchmark_concurrent_ops() {
    Pool* pool = pool_create(sizeof(int), CONCURRENT_THREADS * 2);
    pthread_t threads[CONCURRENT_THREADS];

    // 预热（可选）
    for (int i = 0; i < 100; ++i) {
        int* p = pool_alloc(pool);
        pool_free(pool, p);
    }

    double start = get_high_res_time();
    // 创建线程
    for (int i = 0; i < CONCURRENT_THREADS; ++i) {
        pthread_create(&threads[i], NULL, concurrent_bench_thread, pool);
    }
    // 等待线程完成
    for (int i = 0; i < CONCURRENT_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }
    double duration = get_high_res_time() - start;

    // 结果输出
    const size_t total_ops = CONCURRENT_THREADS * CONCURRENT_OPS_PER_THREAD;
    if(duration < 1e-9) {
        printf("WARNING: benchmark_concurrent_ops() duration is too short, please try again. duration: %f\n", duration);
    } else {
        printf("[Concurrent] %d Threads: %.2f Mops/sec\t(Total: %.2fM ops)\n",
            CONCURRENT_THREADS,
            total_ops / duration / 1e6,
            total_ops / 1e6
        );
    }
    
    // 验证内存一致性
    assert_int_equal(get_alloc_cnt(pool), get_free_cnt(pool));
    pool_destroy(&pool);
}
// =======================================================================================



void entry_generic_pool(void**state) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_basic_operations),
        cmocka_unit_test(test_edge_cases),
        cmocka_unit_test(test_memory_alignment),
        cmocka_unit_test(test_base_run),
        cmocka_unit_test(benchmark_single_thread),
        cmocka_unit_test(benchmark_batch_ops),
        cmocka_unit_test(test_memory_leak),
        cmocka_unit_test(test_stress_memory),
        // cmocka_unit_test(test_concurrent_alloc_free),
        cmocka_unit_test(test_benchmark_concurrent_ops),
    };
    
    cmocka_run_group_tests(tests, NULL, NULL);
}
