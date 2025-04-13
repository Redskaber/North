#pragma once
#include "core/token.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef TEST_TOKEN_POOL_H
#define TEST_TOKEN_POOL_H


TokenBlock* test_get_pool_head(void);
size_t test_get_total_allocated(void);
TaggerPointer test_get_free_list(void);


#endif  // TEST_TOKEN_POOL_H
#ifdef __cplusplus
}
#endif
