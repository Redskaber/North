/**
 * @file token.h
 * @author redskaber (redskaber@foxmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-04-09
 * 
 * @copyright Copyright (c) 2025
 * 
 * @details Token management system with lock-free allocation.
 */

#pragma once
#ifndef __NORTH_TOKEN_H__
#define __NORTH_TOKEN_H__
#include <stdatomic.h>

#include "common.h"
#include "symbol.h"

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
#define ALIGN_AS_CACHELINE      __attribute__((aligned(CACHE_LINE_SIZE)))
#elif defined(_MSC_VER)
#define ALIGN_AS_CACHELINE __declspec(align(ALIGN_DEST_CLSIZE))
#else
#error "Unsupported compiler"
#endif




typedef struct Span { int start; int end; } Span;  // 源码位置
typedef enum Delimiter {
    DELIM_PAREN,    // ()
    DELIM_BRACE,    // {}
    DELIM_BRACKET   // []
} Delimiter;    
typedef enum LitKind {
    LIT_BOOL,          // 布尔字面量（仅AST使用）
    LIT_BYTE,          // 字节字面量 b'x'
    LIT_CHAR,          // 字符字面量 'a'
    LIT_INTEGER,       // 整型字面量 123u32
    LIT_FLOAT,         // 浮点字面量 1.0f32
    LIT_STR,           // 字符串字面量 "hello"
    LIT_STR_RAW,       // 原始字符串 r###"..."###
    LIT_BYTE_STR,      // 字节字符串 b"bytes"
    LIT_BYTE_STR_RAW,  // 原始字节字符串 br###"..."###
    LIT_CSTR,          // C字符串 c"string"
    LIT_CSTR_RAW,      // 原始C字符串 cr###"..."### 
    LIT_ERR            // 错误字面量（带错误保证）
} LitKind;
typedef struct Literal {
    LitKind kind;           // 字面量类型
    Symbol symbol;          // 符号表索引
    Symbol suffix;          // 类型后缀（例如 u8/f32）
    union {
        bool bool_val;      // 布尔值
        char char_val;      // 字符值
        uint8_t byte_val;   // 字节值
        int64_t int_val;    // 整数值
        double float_val;   // 浮点值
        struct {            // 字符串字面量
            char* ptr;
            size_t len;
        } str;
        struct {            // 原始字符串字面量
            char* ptr;
            size_t len;
            uint8_t num_hashes;
        } raw_str;
        struct {            // 错误字面量
            uint32_t error_code;
            char* message;
        } error;
    } as;
} Literal;
typedef struct Ident {
    Symbol symbol;          // 符号表索引
    bool is_raw;            // 是否原始标识符
    Span span;              // 源码位置
} Ident;
typedef enum CommentKind {
    COMMENT_LINE,           // //
    COMMENT_BLOCK           // /** */
} CommentKind;
typedef enum TokenKind {
    Tk_Eq,              // `=`
    Tk_Lt,              // <
    Tk_Le,              // `<=`
    Tk_EqEq,            // `==`
    Tk_Ne,              // `!=`
    Tk_Ge,              // `>=`
    Tk_Gt,              // >` 
    Tk_AndAnd,          // `&&`
    Tk_OrOr,            // `||`
    Tk_Bang,            // `!`
    Tk_Tilde,           // `~`
    Tk_Plus,            // `+`
    Tk_Minus,           // `-`
    Tk_Star,            // `*`
    Tk_Slash,           // `/`
    Tk_Percent,         // `%`
    Tk_Caret,           // `^`
    Tk_And,             // `&`
    Tk_Or,              // `|`
    Tk_Shl,             // `<<`
    Tk_Shr,             // `>>`      
    Tk_PlusEq,          // `+=`
    Tk_MinusEq,         // `-=`
    Tk_StarEq,          // `*=`
    Tk_SlashEq,         // `/=`
    Tk_PercentEq,       // `%=`
    Tk_CaretEq,         // `^=`
    Tk_AndEq,           // `&=`
    Tk_OrEq,            // `|=`
    Tk_ShlEq,           // `<<=`
    Tk_ShrEq,           // `>>=`

    /* Structural symbols */
    Tk_At,              // @
    Tk_Dot,             // .
    Tk_DotDot,          // ..
    Tk_DotDotDot,       // ...
    Tk_DotDotEq,        // ..=
    Tk_Comma,           // ,
    Tk_Semi,            // ;
    Tk_Colon,           // :
    Tk_PathSep,         // ::
    Tk_RArrow,          // ->
    Tk_LArrow,          // <-
    Tk_FatArrow,        // =>
    Tk_Pound,           // #
    Tk_Dollar,          // $
    Tk_Question,        // ？
    Tk_SingleQuote,     // '
    Tk_OpenDelim,       // OpenDelim(Delimiter) to token::Delimiter (e.g., `{`).
    Tk_CloseDelim,      // CloseDelim(Delimiter) to token::Delimiter (e.g., `}`).
    Tk_Literal,         // Litrel(Lit) to token::Lit
    Tk_Ident,           // Ident(Symbol, IdentIsRaw) to token::Ident
    Tk_NtIdent,         // NtIdent(Ident, IdentIsRaw) to token::Ident
    Tk_Lifetime,        // Lifetime(Symbol, IdentIsRaw) to {token::Ident, token::Symbol}
    Tk_NtLifetime,      // NtLifetime(Ident, IdentIsRaw) to {token::Ident, token::Symbol}
    Tk_Interpolated,    // Interpolated(Nonterminal) to token::Nonterminal
    Tk_DocComment,      // DocComment(CommentKind, AttrStyle, Symbol) to {token::Comment, token::AttrStyle, token::Symbol}
    Tk_Error,
    Tk_Eof,             // End of file
} TokenKind;
typedef union TokenData {
    struct { Delimiter delim; } delim;          // 分隔符
    Literal literal;                            // 字面量
    Ident ident;                                // 标识符
    struct {
        CommentKind kind;
        int attr_style;
        Symbol symbol;
    } doc_comment;                              // 文档注释
} TokenData;
typedef struct ALIGN_AS_CACHELINE Token {
    struct Token* next_free;    // 仅用于空闲链表   8 bytes
    TokenKind type;
    Span span;                  // 源码位置
    TokenData data;             // 具体数据
} Token;


// per1
// typedef struct PointerVersion {
//     void* ptr;             
//     uintptr_t version;    
// } PointerVersion;

// per2
// 使用指针标记技术打包版本号
/*[pointer 63-6,  version 6-0] */
// #define VERSION_BITS        6                            // 6bits 版本号
// #define POINTER_BITS        (64 - VERSION_BITS)          // 58bits 指针
// #define VERSION_MASK        ((1UL << VERSION_BITS) - 1)  // (10 0000 0000 -1) => (0011 1111) 6bits
// #define POINTER_MASK        (~VERSION_MASK)              // ([63]1...[6]1 0 0000 0000  58bits

//part3 混合版本指针结构
// typedef struct SafePointer {
//     union {
//         _Atomic(uint64_t) packed;
//         struct {
//             uint64_t ptr    : PTR_BITS;
//             uint64_t epoch  : EPOCH_BITS;
//             uint64_t version: VERS_BITS;
//         };
//     };    
// } SafePointer;

// part4: cx16
typedef struct __attribute__((aligned(16))) {
    uint64_t ptr;
    uint64_t ver;
} TaggerPointer;
typedef _Atomic TaggerPointer atomic_tp;


#define TOKEN_POOL_BLOCK    1024                // 1024个Token
typedef struct TokenBlock {
    Token* block ALIGN_AS_CACHELINE;            // Token指针
    size_t used;                                // 已使用的Token数量
    struct TokenBlock* next;                    // 下一个TokenBlock
    uint8_t _pad[CACHE_LINE_SIZE - sizeof(Token*) - sizeof(size_t) - sizeof(void*)];
} TokenBlock;
typedef _Atomic(TokenBlock*) atomic_tbp;



void token_pool_init(size_t capacity);
void token_pool_cleanup(void);

Token* token_alloc(TokenKind type, Span span);
void token_free(Token* token);

Token* create_literal(Literal lit, Span span);
Token* create_ident(Ident ident, Span span);
Token* create_delim(Delimiter delim, bool is_open, Span span);



// 内存池原子指针
extern atomic_tp free_list;
extern atomic_tbp pool_head;
extern atomic_size_t total_allocated;



#endif  // __NORTH_TOKEN_H__

