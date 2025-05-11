/**
 * @file pool.h
 * @author redskaber (redskaber@foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-09
 * 
 * @copyright Copyright (c) 2025
 * 
 * @details this file is used language io mod.
 *  used generic atomic pool impl in the future.
 */

#pragma once
#ifndef __NORTH_POOL_H__
#define __NORTH_POOL_H__

#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#include <malloc.h>
#define aligned_alloc(align, size) _aligned_malloc(size, align)
#define aligned_free _aligned_free
#else
#include <malloc.h>
#define aligned_free free
#endif



// 架构自适应配置
#if defined(__x86_64__) || defined(_M_X64)
#define CACHE_LINE_SIZE       64
#define ENABLE_ADVANCED_ATOMICS 1
#elif defined(__aarch64__)
#define CACHE_LINE_SIZE       64
#define ENABLE_ADVANCED_ATOMICS 1
#elif defined(__powerpc__)
#define CACHE_LINE_SIZE       128
#else
#define CACHE_LINE_SIZE       64
#endif


// 结构体对齐
#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED_CACHE_LINE __attribute__((aligned(CACHE_LINE_SIZE)))
#elif defined(_MSC_VER)
#define ALIGNED_CACHE_LINE __declspec(align(CACHE_LINE_SIZE))
#else
#error "Unsupported compiler"
#endif

#define ALIGNED_16 __attribute__((aligned(16)))


typedef struct ALIGNED_16 TaggedPointer {
    uintptr_t ptr;      // 低48位存储指针地址
    uint64_t ver;       // 版本号防止ABA问题
} TaggedPointer;

#define tagged_pointer_size             16
#define tagged_pointer_init(PTR, VER)   ((TaggedPointer){.ptr = (uint64_t)(PTR), .ver = VER})
typedef _Atomic(TaggedPointer) atomic_tp_t;

/**
    Pool: 
        {chunk, chunk, chunk,...}
        chunk：
            {node, node, node, ...}
*/

typedef struct ALIGNED_CACHE_LINE Node {
    atomic_tp_t next ALIGNED_16;    // 原子指针+版本号
    uint8_t data[];                 // 用户数据区
} Node;

typedef struct ALIGNED_CACHE_LINE Pool {
    void* memory;              // 预分配内存块
    atomic_tp_t head;          // 无锁队列头（独立缓存行）
    atomic_tp_t free_list;     // 缓存释放对象的链表
    size_t node_size;          // 对齐后的对象大小+头信息
    size_t capacity;           // 池容量
    atomic_uintptr_t alloc_cnt;   // 分配计数器（调试用）
    atomic_uintptr_t free_cnt;    // 释放计数器（调试用）
    atomic_uintptr_t contention_counter;   // 冲突计数器（调试用）
    struct {
        atomic_uintptr_t cas_success;       // cas成功计数器（调试用）
        atomic_uintptr_t cas_fail;          // cas失败计数器（调试用）
        atomic_uintptr_t cache_hit;         // 命中计数器（调试用）
    } stats ALIGNED_CACHE_LINE;
} Pool;

// cas
#define UPDATE_CAS_STATS(pool, success) \
    atomic_fetch_add_explicit(          \
    (success) ? &(pool)->stats.cas_success : &(pool)->stats.cas_fail \
    , 1, memory_order_relaxed           \
    )


// 内存对齐计算宏
#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))
#define NODE_DATA_SIZE(obj_size) ALIGN_UP(obj_size, 16)
#define NODE_TOTAL_SIZE(obj_size) ALIGN_UP(16 + NODE_DATA_SIZE(obj_size), CACHE_LINE_SIZE)


// 静态断言确保结构体对齐
static_assert(sizeof(Node) % CACHE_LINE_SIZE == 0,
    "Node structure requires cache line alignment");

static_assert(offsetof(Pool, stats) % CACHE_LINE_SIZE == 0,
    "Statistics must be cache line aligned");


Pool* pool_create(size_t obj_size, size_t capacity);
void* pool_alloc(Pool* pool);
void pool_free(Pool* pool, void* data);
size_t pool_alloc_batch(Pool* pool, void** objs, size_t count);
void pool_free_batch(Pool* pool, void** objs, size_t count);
void show_pool_info(Pool* pool);
void pool_flush_cache(Pool* pool);
void pool_destroy(Pool** pool_ptr);

void pool_start_reaper(Pool* pool);
void pool_stop_reaper(Pool* pool);


#endif // __NORTH_POOL_H__

