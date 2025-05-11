// nonterminal.h
#pragma once
#ifndef __NORTH_NONTERMINAL_H__
#define __NORTH_NONTERMINAL_H__



#include "common.h"
#include "lexer/symbol.h"

// 非终结符类型枚举
typedef enum NonterminalKind {
    NT_ITEM,
    NT_BLOCK,
    NT_STMT,
    NT_PAT,
    NT_EXPR,
    NT_TY,
    NT_IDENT,
    NT_LIFETIME,
    NT_LITERAL,
    NT_META,
    NT_PATH,
    NT_VIS,
    NT_TT
} NonterminalKind;

// 模式类型子分类
typedef enum NtPatKind {
    PAT_IDENT,
    PAT_STRUCT,
    PAT_TUPLE,
    PAT_LIT,
    PAT_RANGE
} NtPatKind;

// 表达式类型子分类
typedef enum NtExprKind {
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LIT,
    EXPR_CALL,
    EXPR_MACRO
} NtExprKind;

typedef struct {
    Symbol symbol;
    struct ASTBlock* block;
} NtPatIdent;

typedef struct Span {
    int end;
    int start;
} Span;

// 通用非终结符结构
// typedef struct ALIGN_AS_CACHELINE Nonterminal {
//     _Atomic uint32_t refcount;  // 引用计数
//     NonterminalKind kind;       // 类型标记
//     Span span;                  // 源码位置
    
//     union {
//         struct ASTBlock* block; // NT_BLOCK对应的AST块
//         struct {
//             NtPatKind pat_kind; // NT_PAT的子类型
//             void* pat_data;     // 模式数据指针
//         };
//         struct {
//             NtExprKind expr_kind; // NT_EXPR的子类型
//             void* expr_data;      // 表达式数据指针
//         };
//         Symbol symbol;          // 符号类数据（IDENT/LITERAL等）
//     } data;
// } Nonterminal;



// // 内存管理接口
// Nonterminal* nt_alloc(NonterminalKind kind, Span span);
// void nt_ref(Nonterminal* nt);
// void nt_unref(Nonterminal* nt);

// // 具体类型构造器
// Nonterminal* nt_block(struct ASTBlock* block, Span span);
// Nonterminal* nt_pat(NtPatKind pat_kind, void* pat_data, Span span);
// Nonterminal* nt_expr(NtExprKind expr_kind, void* expr_data, Span span);
// Nonterminal* nt_symbol(NonterminalKind kind, Symbol sym, Span span);

// 线程安全访问接口
// Nonterminal* nt_clone(const Nonterminal* orig);

#endif // __NORTH_NONTERMINAL_H__
