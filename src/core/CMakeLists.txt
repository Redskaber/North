# 📁 src/core/CMakeLists.txt
# 核心库定义
add_library(north_core STATIC
    io/io.c
    lexer/lexer.c
    lexer/nonterminal.c
    lexer/symbol.c
    lexer/token.c
    pool/pool.c
)

# 包含目录配置
target_include_directories(north_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/core>
    $<INSTALL_INTERFACE:include>
    ${LLVM_INCLUDE_DIRS}
)

# 编译器选项
set_target_compile_options(north_core)

# 依赖配置
target_link_libraries(north_core PUBLIC
    ${LLVM_LIBS}
    LibEdit::LibEdit
    CURL::libcurl
    atomic pthread curses tinfo xml2 ffi
)

# Sanitizer配置（仅Debug模式）
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(north_core PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    
    target_link_libraries(north_core PUBLIC
        $<$<CONFIG:Debug>:-lasan -lubsan>
        ${CMAKE_DL_LIBS} 
    )
endif()

# 单元测试宏定义
target_compile_definitions(north_core PRIVATE
    $<$<BOOL:${UNIT_TESTING}>:UNIT_TESTING=1>
)



