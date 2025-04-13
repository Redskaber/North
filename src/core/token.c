#ifdef UNIT_TESTING
#undef static
#define static 
#endif

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdalign.h>
#include <stdint.h>

#include "token.h"



// 静态断言验证内存布局
static_assert((sizeof(Token) % CACHE_LINE_SIZE) == 0, 
    "Token size must exactly match cache line");
static_assert(offsetof(Token, next_free) % 1 == 0,
    "next_free must be first field for atomic ops");
static_assert(sizeof(TokenBlock) % CACHE_LINE_SIZE == 0,
    "TokenBlock alignment violation");




atomic_tp free_list;
atomic_tbp pool_head = NULL;
atomic_size_t total_allocated = 0;

#ifdef DEBUG
atomic_uint_fast64_t version_wrap_count = 0;
#endif



void token_pool_init(size_t capacity) {
    if(pool_head) return;

    size_t block_count = (capacity + TOKEN_POOL_BLOCK - 1) / TOKEN_POOL_BLOCK;
    
    TokenBlock* head = NULL;
    for (size_t i = 0; i < block_count; ++i) {
        TokenBlock* new_block = aligned_alloc(CACHE_LINE_SIZE, sizeof(TokenBlock));
        new_block->block = aligned_alloc(CACHE_LINE_SIZE, 
            sizeof(Token) * TOKEN_POOL_BLOCK);
        new_block->used = 0;
        new_block->next = head;
        head = new_block;
    }
    atomic_init(&free_list, ((TaggerPointer){.ptr = 0, .ver = 0}));
    atomic_init(&pool_head, head);
    atomic_init(&total_allocated, 0);
}

void token_pool_cleanup(void) {
    // 释放所有TokenBlock
    TokenBlock* current = atomic_exchange_explicit(&pool_head, NULL, memory_order_acquire);
    while (current) {
        TokenBlock* next = current->next;
        free(current->block);    // 先释放Token数组
        free(current);           // 再释放TokenBlock结构体
        current = next;
    }

    // 重置原子变量（C11 atomic_init不需要锁）
    atomic_init(&free_list, (TaggerPointer){0});
    atomic_init(&total_allocated, 0);
    atomic_init(&pool_head, NULL);

#if defined(DEBUG)
    atomic_init(&version_wrap_count, 0);
#endif
}

Token* token_alloc(TokenKind type, Span span) {
    TaggerPointer old_packed, new_packed;
    Token* desired = NULL;

    // 快速路径
    do {
        old_packed = atomic_load_explicit(&free_list, memory_order_acquire);
        desired = (Token*)(old_packed.ptr);
        if (!desired) break;

        uint64_t ver = old_packed.ver + 1;
        new_packed = ((TaggerPointer){.ptr = (uint64_t)(desired->next_free), .ver = ver});
    } while (!atomic_compare_exchange_weak_explicit(
        &free_list,
        &old_packed, 
        new_packed,
        memory_order_acq_rel, 
        memory_order_acquire
    ));

    // 慢速路径
    if (!desired) {
        TokenBlock* head = atomic_load_explicit(&pool_head, memory_order_acquire);
        if (!head || head->used >= TOKEN_POOL_BLOCK) {
            TokenBlock* new_block = aligned_alloc(CACHE_LINE_SIZE, 
                sizeof(TokenBlock));
            new_block->block = aligned_alloc(CACHE_LINE_SIZE, 
                sizeof(Token) * TOKEN_POOL_BLOCK);
            new_block->used = 0;
            new_block->next = head;
            atomic_store_explicit(&pool_head, new_block, memory_order_release);
            head = new_block;
        }

        __builtin_prefetch(head->block + head->used);
        desired = &head->block[head->used++];
        atomic_fetch_add_explicit(&total_allocated, 1, memory_order_relaxed);
    }

    Token init_token = {
        .type = type,
        .span = span,
        .next_free = NULL,
    };
    memcpy(desired, &init_token, sizeof(Token));
    
    return desired;
}

void token_free(Token* token) {
    assert((uintptr_t)token % CACHE_LINE_SIZE == 0);
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    if ((uintptr_t)token->next_free & (CACHE_LINE_SIZE-1)) {
        __builtin_trap();
    }
#endif

    TaggerPointer old_packed, new_packed;
    do {
        memset(token, 0, sizeof(Token));

        old_packed = atomic_load_explicit(&free_list, memory_order_acquire);
        token->next_free = (Token*)(old_packed.ptr);
        uint64_t ver = old_packed.ver + 1;
        new_packed = ((TaggerPointer){.ptr = (uint64_t)(token), .ver = ver});
    } while (!atomic_compare_exchange_weak_explicit(
        &free_list,
        &old_packed, 
        new_packed,
        memory_order_acq_rel,   // 成功时的内存序
        memory_order_acquire    // 失败时的内存序
    ));
}

// 创建字面量Token
Token* create_literal(Literal lit, Span span) {
    Token* token = token_alloc(Tk_Literal, span);
    token->data.literal = lit;
    return token;
}


// 创建标识符Token
Token* create_ident(Ident ident, Span span) {
    Token* token = token_alloc(Tk_Ident, span);
    token->data.ident = ident;
    return token;
}

// 创建分隔符Token
Token* create_delim(Delimiter delim, bool is_open, Span span) {
    Token* token = token_alloc(is_open ? Tk_OpenDelim : Tk_CloseDelim, span);
    token->data.delim.delim = delim;
    return token;
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
TEST_API TokenBlock* test_get_pool_head(void) {
    return atomic_load_explicit(&pool_head, memory_order_acquire);
}

TEST_API size_t test_get_total_allocated(void) {
    return atomic_load_explicit(&total_allocated, memory_order_relaxed);
}

TEST_API TaggerPointer test_get_free_list(void) {
    return atomic_load_explicit(&free_list, memory_order_acquire);
}

#endif

