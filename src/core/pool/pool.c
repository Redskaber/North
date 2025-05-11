#ifdef UNIT_TESTING
#undef static
#define static 
#endif
/*TODO:
1. CAS竞争优化
2. 内存序优化
3. 缓存策略增强
4. NUMA深度优化
5. 数据结构优化

1. 层级化批量操作
2. 基于硬件特性的优化
3. 智能预取策略

建议实施顺序：
    自适应退避策略 + 内存序优化 （低风险/高收益）
    层级化缓存改造 （中风险）
    NUMA深度优化 （针对服务器环境）
    TSX等硬件优化 （后期精细优化）
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdalign.h>
#include <stdint.h>


#include "pool/pool.h"


#define POOL_TOTAL_SIZE(obj_size, init_cap) \
    (sizeof(Pool) + NODE_TOTAL_SIZE(obj_size) * capacity)
#define BATCH_SIZE              64
#define CACHE_SIZE              256
#define CACHE_LOW_THRESHOLD     0.25
#define CACHE_HIGH_THRESHOLD    0.75
#define CACHE_LOW_WATERMARK     (CACHE_LOW_THRESHOLD * CACHE_SIZE)
#define CACHE_HIGH_WATERMARK    (CACHE_HIGH_THRESHOLD * CACHE_SIZE)
#define PAD_THREAD      ((CACHE_LINE_SIZE - sizeof(size_t)) % CACHE_SIZE*sizeof(void*))
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

typedef struct {
    void* cache[CACHE_SIZE];
    size_t count;
    uint8_t _pad[PAD_THREAD];
} ThreadCache;

static __thread ThreadCache thead_cache = { .count = 0 };


static atomic_bool reaper_run = ATOMIC_VAR_INIT(false);
static pthread_t reaper_tid;

void* reaper_thread(void* arg) {
    Pool* pool = (Pool*)arg;
    while (atomic_load(&reaper_run)) {
        nanosleep(&(struct timespec){0, 1000000}, NULL);    // 1ms
        pool_flush_cache(pool);
    }
    return NULL;
}
void pool_start_reaper(Pool* pool) {
    atomic_store(&reaper_run, true);
    pthread_create(&reaper_tid, NULL, reaper_thread, pool);
}
void pool_stop_reaper(Pool* pool) {
    (void)pool;
    atomic_store(&reaper_run, false);
    pthread_join(reaper_tid, NULL);
}


// 内存屏障包装
// 结合标准库和汇编指令的混合方案
#include <stdatomic.h>

// 编译器屏障统一实现
#define COMPILER_BARRIER() atomic_signal_fence(memory_order_seq_cst)

// 架构特定的CPU屏障
#if defined(__x86_64__) || defined(_M_X64)
#define CPU_MEMORY_BARRIER() asm volatile("mfence" ::: "memory")
#elif defined(__aarch64__)
#define CPU_MEMORY_BARRIER() asm volatile("dmb ish" ::: "memory")
#elif defined(__powerpc__)
#define CPU_MEMORY_BARRIER() asm volatile("sync" ::: "memory")
#else
#define CPU_MEMORY_BARRIER() atomic_thread_fence(memory_order_seq_cst)
#endif

// 综合内存屏障
#define MEMORY_BARRIER() do { \
    COMPILER_BARRIER(); \
    CPU_MEMORY_BARRIER(); \
    COMPILER_BARRIER(); \
} while(0)

// 根据内存序需求选择屏障强度
#define LIGHT_BARRIER() atomic_thread_fence(memory_order_acq_rel)
#define FULL_BARRIER()  MEMORY_BARRIER()
#if defined(__aarch64__)
// 优化屏障类型选择
#define LOAD_BARRIER()  asm volatile("dmb ld" ::: "memory")
#define STORE_BARRIER() asm volatile("dmb st" ::: "memory")
#endif


// 增加退避策略和内存序优化
#if defined(__x86_64__) || defined(_M_X64)
#define ALLOC_MO memory_order_acquire
#define FREE_MO  memory_order_release
#elif defined(__aarch64__)
#define ALLOC_MO memory_order_consume
#define FREE_MO  memory_order_release
#else
#define ALLOC_MO memory_order_acq_rel
#define FREE_MO  memory_order_acq_rel
#endif



Pool* pool_create(size_t obj_size, size_t capacity) {
    if (capacity == 0 || obj_size == 0) {
        errno = EINVAL;
        return NULL;
    }
    const size_t node_size = NODE_TOTAL_SIZE(obj_size);
    const size_t total_size = POOL_TOTAL_SIZE(obj_size, capacity);

    Pool* pool = aligned_alloc(CACHE_LINE_SIZE, total_size);
    if (!pool) return NULL;

    pool->memory = (uint8_t*)pool + ALIGN_UP(sizeof(Pool), CACHE_LINE_SIZE);
    pool->node_size = node_size;
    pool->capacity = capacity;

    atomic_init(&pool->alloc_cnt, 0);
    atomic_init(&pool->free_cnt, 0);
    atomic_init(&pool->contention_counter, 0);

    // 初始化空闲链表（LIFO）
    Node* prev = NULL;
    for (size_t i = 0; i < capacity; ++i) {
        Node* curr = (Node*)((uint8_t*)pool->memory + i * node_size);
        atomic_init(&curr->next, tagged_pointer_init(prev, 0));
        prev = curr;
    }

    // 初始化头尾指针
    atomic_init(&pool->head, tagged_pointer_init(prev, 0));
    atomic_init(&pool->free_list, tagged_pointer_init(0, 0));

    return pool;
}

static bool cas_internal(Pool* pool_ptr, atomic_tp_t* target, TaggedPointer* expected, TaggedPointer desired) {
    bool cas_flag = atomic_compare_exchange_weak_explicit(
        target, expected, desired,
        ALLOC_MO,
        memory_order_relaxed
    );
    UPDATE_CAS_STATS(pool_ptr, cas_flag);
    return cas_flag;
}

void* pool_alloc(Pool* pool) {
    atomic_tp_t* target_list = &pool->free_list;
    TaggedPointer old_head, new_head;
    Node* chunk = NULL;

    old_head = atomic_load_explicit(target_list, ALLOC_MO);
    if (!old_head.ptr) {
        target_list = &pool->head;
        old_head = atomic_load_explicit(target_list, ALLOC_MO);
    }

    do {
        if (!old_head.ptr) return NULL;
        chunk = (Node*)old_head.ptr;
        TaggedPointer next = atomic_load_explicit(&chunk->next, memory_order_relaxed);

#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch((void*)next.ptr);
#endif

        new_head.ptr = next.ptr;
        new_head.ver = old_head.ver + 1;     
    } while (!cas_internal(pool, target_list, &old_head, new_head));

    atomic_fetch_add_explicit(&pool->alloc_cnt, 1, memory_order_relaxed);
    return chunk->data;
}



void pool_free(Pool* pool, void* data) {
    assert(data != NULL && "Cannot free NULL pointer");

    Node* node = (Node*)((uint8_t*)data - tagged_pointer_size);
    assert((uint8_t*)node->data == data && "Pointer calculation error");
    assert(((uintptr_t)node % CACHE_LINE_SIZE) == 0);

    TaggedPointer old_head, new_head;
    do {
        old_head = atomic_load_explicit(&pool->free_list, memory_order_acquire);
        atomic_store_explicit(
            &node->next,
            tagged_pointer_init(old_head.ptr, old_head.ver + 1),
            memory_order_relaxed
        );

        new_head.ptr = (uintptr_t)node;
        new_head.ver = old_head.ver + 1;
    } while (!atomic_compare_exchange_weak_explicit(
            &pool->free_list, &old_head, new_head,
            FREE_MO,
            memory_order_relaxed
    ));

    atomic_fetch_add_explicit(&pool->free_cnt, 1, memory_order_relaxed);
}


// 内部批量分配实现
static size_t batch_alloc_internal(Pool* pool, void** objs, size_t count) {
    size_t remaining = pool->capacity - (atomic_load(&pool->alloc_cnt) - atomic_load(&pool->free_cnt));
    if (remaining == 0) return 0;
    count = MIN(count, remaining);

    TaggedPointer old_head, new_head;
    Node* chunks[BATCH_SIZE];
    size_t allocated = 0;
    atomic_tp_t* target_list = &pool->free_list;

    while (allocated < count) {
        size_t request = MIN(BATCH_SIZE, count - allocated);
        size_t obtained = 0;
        do {
            old_head = atomic_load_explicit(target_list, memory_order_acquire);
            if (!old_head.ptr) {
                target_list = &pool->head;
                old_head = atomic_load_explicit(target_list, memory_order_acquire);
                if (!old_head.ptr) break;
            }

            Node* current = (Node*)old_head.ptr;
            for (obtained = 0; obtained < request && current; ++obtained) {
                chunks[obtained] = current;
                TaggedPointer next = atomic_load_explicit(&current->next, memory_order_relaxed);
                current = (Node*)next.ptr;

#if defined(__GNUC__) || defined(__clang__)
                if (obtained % 4 == 0) {
                    __builtin_prefetch(current);
                }
#endif
            }

            new_head.ptr = (uintptr_t)current;
            new_head.ver = old_head.ver + 1;
        } while (!cas_internal(pool, target_list, &old_head, new_head));

        for (size_t i = 0; i < obtained; ++i) {
            objs[allocated++] = chunks[i]->data;
        }
        atomic_fetch_add_explicit(&pool->alloc_cnt, obtained, memory_order_relaxed);
        atomic_fetch_add_explicit(&pool->contention_counter, obtained > 0 ? 0 : 1, memory_order_relaxed);
    }

    return allocated;
}


size_t pool_alloc_batch(Pool* pool, void** objs, size_t count) {
    if (!objs || count == 0) return 0;
    
    size_t available = pool->capacity - (atomic_load(&pool->alloc_cnt) - atomic_load(&pool->free_cnt));
    count = MIN(count, available);
    size_t from_cache = MIN(count, thead_cache.count);

    // 先从线程缓存分配
    if (from_cache > 0) {
        memcpy(objs, &thead_cache.cache[thead_cache.count - from_cache], from_cache * sizeof(void*));
        thead_cache.count -= from_cache;
    }
    
    // 剩余数量走批量分配
    size_t batch_allocated = batch_alloc_internal(pool, objs + from_cache, count - from_cache);
    size_t total_allocated = from_cache + batch_allocated;

    // 处理超额分配（填充本地缓存）
    if (total_allocated > count) {
        size_t excess = total_allocated - count;
        size_t cache_space = CACHE_SIZE - thead_cache.count;
        size_t to_cache = MIN(excess, cache_space);

        memcpy(&thead_cache.cache[thead_cache.count], objs + count, to_cache * sizeof(void*));
        thead_cache.count += to_cache;

        // 释放无法缓存的部分
        if (excess > to_cache) {
            pool_free_batch(pool, objs + count + to_cache, excess - to_cache);
        }
    }

    return MIN(total_allocated, count);
}


// 批量释放函数
static void pool_free_batch_internal(Pool* pool, void** objs, size_t count) {
    if(count == 0) return;

    // 2.剩余对象批量释放到全局池
    Node* chunks[BATCH_SIZE];
    size_t processed = 0;

    while (processed < count) {
        size_t batch = MIN(BATCH_SIZE, count - processed);
        TaggedPointer old_head, new_head;

        for (size_t i = 0; i < batch; ++i) {
            Node* node = (Node*)((uint8_t*)objs[processed + i] - offsetof(Node, data));
            chunks[i] = node;
        }

        // 逆序链接节点以保持LIFO
        for (size_t i = batch-1; i > 0; --i) {
            atomic_store_explicit(
                &chunks[i]->next,
                tagged_pointer_init(chunks[i-1], 0),
                memory_order_relaxed
            );
        }

        do {
            old_head = atomic_load_explicit(&pool->free_list, memory_order_acquire);
            atomic_store_explicit(
                &chunks[batch-1]->next,
                old_head,
                memory_order_relaxed
            );

            new_head.ptr = (uintptr_t)chunks[0];
            new_head.ver = old_head.ver + 1;
        } while (!atomic_compare_exchange_weak_explicit(
                &pool->free_list, &old_head, new_head,
                memory_order_release,
                memory_order_relaxed
        ));

        processed += batch;
        atomic_fetch_add_explicit(&pool->free_cnt, batch, memory_order_relaxed);
    }
}

// 批量释放函数
void pool_free_batch(Pool* pool, void** objs, size_t count) {
    if (!objs || count == 0) return;
    
    if (thead_cache.count >= CACHE_HIGH_WATERMARK) {
        size_t release = thead_cache.count - CACHE_HIGH_WATERMARK;
        pool_free_batch_internal(pool, thead_cache.cache, release);
        thead_cache.count -= release;
        memmove(
            thead_cache.cache, 
            &thead_cache.cache[release], 
            thead_cache.count * sizeof(void*)
        );
    }

    // 1.尝试缓存释放对象
    size_t to_cache = MIN(count, CACHE_SIZE - thead_cache.count);
    if (to_cache > 0) {
        memcpy(&thead_cache.cache[thead_cache.count], objs, to_cache * sizeof(void*));
        thead_cache.count += to_cache;
        count -= to_cache;
        objs += to_cache;
    }
    if (count > 0) {
        pool_free_batch_internal(pool, objs, count);
    }
}


// 刷新缓存
void pool_flush_cache(Pool* pool) {
    if (thead_cache.count > 0) {
        pool_free_batch_internal(pool, thead_cache.cache, thead_cache.count);
        thead_cache.count = 0;
    }
}


void show_pool_info(Pool* pool) {
    printf("Pool Info:\n");
    printf("  Memory: %p\n", pool->memory);
    printf("  Capacity: %zu\n", pool->capacity);
    printf("  Node Size: %zu\n", pool->node_size);
    printf("  Allocated: %zu\n", atomic_load_explicit(&pool->alloc_cnt, memory_order_relaxed));
    printf("  Free: %zu\n", atomic_load_explicit(&pool->free_cnt, memory_order_relaxed));
    printf("  Contention Counter: %zu\n", atomic_load_explicit(&pool->contention_counter, memory_order_relaxed));
    size_t cas_success = atomic_load(&pool->stats.cas_success);
    size_t cas_fail = atomic_load(&pool->stats.cas_fail);
    printf("  CAS(su/per): %.2f%%\n", 
        100.0 * (double) cas_success/ ((double)cas_success + (double)cas_fail)
    );
}

void pool_destroy(Pool** pool_ptr) {
    if(!pool_ptr) return;
    Pool* pool = *pool_ptr;
    if (pool) {
        pool_flush_cache(pool);
        // 验证所有对象已回收
        uintptr_t alloc = atomic_load_explicit(&pool->alloc_cnt, memory_order_acquire);
        uintptr_t freed = atomic_load_explicit(&pool->free_cnt, memory_order_acquire);

        if (alloc != freed) {
#ifdef _WIN32
            fprintf(stderr, "[ERROR] Pool leak detected! alloc: %I64u, freed: %I64u\n", alloc, freed);
#else
            fprintf(stderr, "[ERROR] Pool leak detected! alloc: %lu, freed: %lu\n", alloc, freed);
#endif
            abort();
        }

        aligned_free(pool);
        *pool_ptr = NULL;
    }
}



// 添加测试接口实现
#ifdef UNIT_TESTING
#if defined(__GNUC__) || defined(__clang__)
#define TEST_API __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define TEST_API __declspec(dllexport)
#else
#define TEST_API
#endif

TEST_API size_t get_alloc_cnt(Pool* pool) {
    return atomic_load_explicit(&pool->alloc_cnt, memory_order_relaxed);
}

TEST_API size_t get_free_cnt(Pool* pool) {
    return atomic_load_explicit(&pool->free_cnt, memory_order_relaxed);
}

TEST_API size_t get_contention_counter(Pool* pool) {
    return atomic_load_explicit(&pool->contention_counter, memory_order_relaxed);
}

#endif 




