#pragma once
#include "pool/pool.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifndef TEST_NORTH_POOL_H
#define TEST_NORTH_POOL_H


size_t get_alloc_cnt(Pool* pool);
size_t get_free_cnt(Pool* pool);
size_t get_contention_counter(Pool* pool);


#endif  // TEST_NORTH_POOL_H
#ifdef __cplusplus
}
#endif
