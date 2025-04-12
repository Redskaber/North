#pragma once
#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef TEST_TOKEN_POOL_H
#define TEST_TOKEN_POOL_H
// 声明测试接口
TokenBlock* test_get_pool_head(void);
size_t test_get_total_allocated(void);
TaggerPointer test_get_free_list(void);
#endif
#ifdef __cplusplus
}
#endif
