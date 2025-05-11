# 📁 cmake/CompilerFlags.cmake
function(set_target_compile_options TARGET)
    target_compile_options(${TARGET} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        -Wall
        -Wextra
        -Werror
        -march=native
        -mcx16
    )
endfunction()

# 编译器版本检查
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 13)
        message(FATAL_ERROR "Requires Clang/GCC >= 13 (found ${CMAKE_C_COMPILER_VERSION})")
    endif()
endif()


