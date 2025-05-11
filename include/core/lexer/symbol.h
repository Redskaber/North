#pragma once

#ifndef __NORTH_SYMBOL_H__
#define __NORTH_SYMBOL_H__
#include <pthread.h>
#include "common.h"

// 符号表条目结构
typedef struct SymbolEntry {
    const char* str;       // 字符串指针
    size_t len;            // 字符串长度
    uint32_t hash;         // 预计算哈希值
    _Atomic uint32_t refcount; // 引用计数
} SymbolEntry;

// 符号表结构
typedef struct SymbolTable {
    SymbolEntry* entries;  // 条目数组
    size_t capacity;       // 总容量
    size_t size;           // 当前大小
    pthread_mutex_t lock;  // 线程安全锁
} SymbolTable;

// 预定义符号ID
typedef enum PredefinedSymbols {
    #define SYM(label, str) SYM_##label,
    // #define SYMBOL_LIST
    #include "symbol_defs.h"   
    SYMBOL_LIST
    #undef SYMBOL_LIST
    #undef SYM
    SYM_COUNT
} PredefinedSymbols;

// 符号类型定义
typedef struct Symbol {
    uint32_t id;        // 符号ID 
    uint8_t flags;      // 标志位（见下方宏定义）
} Symbol;

// 符号标志位（表示符号的类型）
#define SYM_FLAG_PREDEFINED 0x01   // 预定义符号
#define SYM_FLAG_INTERNED   0x02   // 已内部化
#define SYM_FLAG_LIFETIME   0x04   // 生命周期符号

// 符号表API
void symbol_table_init(void);
Symbol symbol_intern(const char* str, size_t len);
const char* symbol_str(Symbol sym);
void symbol_ref(Symbol sym);
void symbol_unref(Symbol sym);

// 预定义符号访问宏
#define MACRO_SYM_EMPTY         (Symbol){SYM_EMPTY, SYM_FLAG_PREDEFINED}
#define MACRO_SYM_ROOT          (Symbol){SYM_ROOT, SYM_FLAG_PREDEFINED}



#endif // __NORTH_SYMBOL_H__    
