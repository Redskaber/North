#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "lexer/symbol.h"


// 预定义符号字符串数组
static const char* predefined_strs[] = {
    #define SYM(id, str) [SYM_##id] = str,
    // #define SYMBOL_LIST
    #include "lexer/symbol_defs.h"
    SYMBOL_LIST
    #undef SYMBOL_LIST
    #undef SYM
};


// 全局符号表实例
static SymbolTable global_symtab = {0};


// FNV-1a哈希算法
static uint32_t fnv1a_hash(const char* str, size_t len) {
    const uint32_t FNV_PRIME = 16777619U;
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint8_t)str[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

void symbol_table_init(void) {
    pthread_mutex_init(&global_symtab.lock, NULL);
    
    // 初始化预定义符号
    global_symtab.capacity = SYM_COUNT * 2;
    global_symtab.entries = calloc(global_symtab.capacity, sizeof(SymbolEntry));
    
    for (PredefinedSymbols id = 0; id < SYM_COUNT; ++id) {
        const char* str = predefined_strs[id];
        size_t len = strlen(str);
        uint32_t hash = fnv1a_hash(str, len);
        
        SymbolEntry entry = {
            .str = str,
            .len = len,
            .hash = hash,
            .refcount = 1
        };
        
        // 直接插入预定义位置
        global_symtab.entries[id] = entry;
    }
    global_symtab.size = SYM_COUNT;
}

Symbol symbol_intern(const char* str, size_t len) {
    if (len == 0) return MACRO_SYM_EMPTY;
    
    uint32_t hash = fnv1a_hash(str, len);
    
    pthread_mutex_lock(&global_symtab.lock);
    
    // 检查预定义符号
    for (PredefinedSymbols id = 0; id < SYM_COUNT; ++id) {
        const SymbolEntry* entry = &global_symtab.entries[id];
        if (entry->len == len && 
            entry->hash == hash &&
            memcmp(entry->str, str, len) == 0) {
            pthread_mutex_unlock(&global_symtab.lock);
            return (Symbol){id, SYM_FLAG_PREDEFINED};
        }
    }
    
    // 查找现有条目
    for (size_t i = SYM_COUNT; i < global_symtab.size; ++i) {
        SymbolEntry* entry = &global_symtab.entries[i];
        if (entry->len == len && 
            entry->hash == hash &&
            memcmp(entry->str, str, len) == 0) {
            atomic_fetch_add_explicit(&entry->refcount, 1, memory_order_relaxed);
            pthread_mutex_unlock(&global_symtab.lock);
            return (Symbol){i, SYM_FLAG_INTERNED};
        }
    }
    
    // 扩容检查
    if (global_symtab.size >= global_symtab.capacity) {
        global_symtab.capacity *= 2;
        global_symtab.entries = realloc(global_symtab.entries, 
            global_symtab.capacity * sizeof(SymbolEntry));
    }
    
    // 创建新条目
    SymbolEntry* entry = &global_symtab.entries[global_symtab.size];
    entry->str = strndup(str, len);
    entry->len = len;
    entry->hash = hash;
    atomic_init(&entry->refcount, 1);
    
    Symbol sym = {global_symtab.size++, SYM_FLAG_INTERNED};
    pthread_mutex_unlock(&global_symtab.lock);
    return sym;
}

const char* symbol_str(Symbol sym) {
    if (sym.flags & SYM_FLAG_PREDEFINED) {
        return predefined_strs[sym.id];
    }
    if (sym.id < global_symtab.size) {
        return global_symtab.entries[sym.id].str;
    }
    return NULL;
}

void symbol_ref(Symbol sym) {
    if (!(sym.flags & SYM_FLAG_INTERNED)) return;
    
    SymbolEntry* entry = &global_symtab.entries[sym.id];
    atomic_fetch_add_explicit(&entry->refcount, 1, memory_order_relaxed);
}

void symbol_unref(Symbol sym) {
    if (!(sym.flags & SYM_FLAG_INTERNED)) return;
    
    SymbolEntry* entry = &global_symtab.entries[sym.id];
    if (atomic_fetch_sub_explicit(&entry->refcount, 1, memory_order_acq_rel) == 1) {
        free((void*)entry->str);
        entry->str = NULL;
    }
}

