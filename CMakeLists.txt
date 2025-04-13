# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(
    North 
    LANGUAGES C
    VERSION 0.1.0 
    DESCRIPTION "Self-compiling C compiler implementation"
)

#----------------------------------------
# 基础配置
#----------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#----------------------------------------
# 项目结构
#----------------------------------------
set(PROJECT_SRC_DIR src)
set(PROJECT_INC_DIR include)
set(MAIN_SOURCE_FILE ${PROJECT_SRC_DIR}/north.c)

#----------------------------------------
# 输出目录配置
#----------------------------------------
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

foreach(TYPE IN ITEMS ARCHIVE LIBRARY RUNTIME)
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set(CMAKE_${TYPE}_OUTPUT_DIRECTORY_${CONFIG_UPPER}
            "${CMAKE_BINARY_DIR}/output/${CONFIG}"
            CACHE PATH "Output directory for ${TYPE}" FORCE
        )
    endforeach()
endforeach()

#----------------------------------------
# 组件开关
#----------------------------------------
option(UNIT_TESTING "Enable unit tests" OFF)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

#----------------------------------------
# 编译器检查
#----------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 13)
        message(FATAL_ERROR "Requires Clang/GCC >= 13 (found ${CMAKE_C_COMPILER_VERSION})")
    endif()
endif()

#----------------------------------------
# 依赖配置
#----------------------------------------
find_package(LLVM 17 REQUIRED COMPONENTS core support analysis)
find_package(LibEdit REQUIRED)
find_package(CURL REQUIRED)

#----------------------------------------
# 核心库配置
#----------------------------------------
file(GLOB_RECURSE CORE_SOURCES
    "${PROJECT_SRC_DIR}/core/*.c"
    "${PROJECT_SRC_DIR}/core/*.h"
)

add_library(north_core STATIC ${CORE_SOURCES})

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    # sanitizer link
    target_link_options(north_core PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    
    # sanitizer runtime
    target_link_libraries(north_core PUBLIC
        $<$<CONFIG:Debug>:-lasan -lubsan>
        ${CMAKE_DL_LIBS} 
    )
endif()

target_include_directories(north_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/core>
    $<INSTALL_INTERFACE:include>
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(north_core PRIVATE
    $<$<BOOL:${UNIT_TESTING}>:UNIT_TESTING=1>
)

target_link_libraries(north_core PUBLIC
    ${LLVM_LIBS}
    LibEdit::LibEdit
    CURL::libcurl
    atomic pthread curses tinfo xml2 ffi
)

target_compile_options(north_core PRIVATE
    $<$<CONFIG:Debug>:-O0 -g3 -fsanitize=address,undefined>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    -Wall
    -Wextra
    -Werror
    -march=native
    -mcx16
)

#----------------------------------------
# 主程序配置
#----------------------------------------
add_executable(north ${MAIN_SOURCE_FILE})
target_link_libraries(north PRIVATE north_core)

#----------------------------------------
# 测试配置
#----------------------------------------
if(UNIT_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

#----------------------------------------
# 安装规则
#----------------------------------------
install(TARGETS north north_core
    EXPORT NorthTargets
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

#----------------------------------------
# 打包配置
#----------------------------------------
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NorthConfig.cmake
    INSTALL_DESTINATION lib/cmake/North
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NorthConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT NorthTargets
    FILE NorthTargets.cmake
    DESTINATION lib/cmake/North
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NorthConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NorthConfigVersion.cmake
    DESTINATION lib/cmake/North
)

#----------------------------------------
# 编译加速
#----------------------------------------
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()


