#include <stdlib.h>
#include <string.h>

#include "lexer/nonterminal.h"

// 对象池配置
// #define NT_POOL_BLOCK 512
// static _Atomic(struct NTBlock*) nt_pool_head;

// typedef struct NTBlock {
//     Nonterminal* block;
//     size_t used;
//     struct NTBlock* next;
//     uint8_t _pad[CACHE_LINE_SIZE - (sizeof(Nonterminal*)+sizeof(size_t)+sizeof(void*))];
// } NTBlock;

// Nonterminal* nt_alloc(NonterminalKind kind, Span span) {
//     // 实现类似token_alloc的内存池分配逻辑
//     // 包含原子操作和池扩容机制
//     // ...
// }

// void nt_ref(Nonterminal* nt) {
//     if (!nt) return;
//     atomic_fetch_add_explicit(&nt->refcount, 1, memory_order_relaxed);
// }

// void nt_unref(Nonterminal* nt) {
//     if (!nt) return;
    
//     if (atomic_fetch_sub_explicit(&nt->refcount, 1, memory_order_acq_rel) == 1) {
//         // 根据类型释放资源
//         switch (nt->kind) {
//             case NT_BLOCK: free_ast_block(nt->data.block); break;
//             case NT_PAT:   free(nt->data.pat_data); break;
//             case NT_EXPR:  free(nt->data.expr_data); break;
//             // 其他类型处理...
//         }
//         return_to_pool(nt); // 回收到内存池
//     }
// }

// // 实现具体构造器（示例：Block类型）
// Nonterminal* nt_block(struct ASTBlock* block, Span span) {
//     Nonterminal* nt = nt_alloc(NT_BLOCK, span);
//     nt->data.block = block;
//     return nt;
// }


