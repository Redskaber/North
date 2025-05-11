# 📁 CMakeLists.txt (根目录)
cmake_minimum_required(VERSION 3.20)
project(
    North 
    LANGUAGES C
    VERSION 0.1.0 
    DESCRIPTION "Self-compiling C compiler implementation"
)

#----------------------------------------
# 全局配置
#----------------------------------------
# 基础配置
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 包含自定义模块
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)
include(cmake/OutputDirectories.cmake)

#----------------------------------------
# 项目模块
#----------------------------------------
add_subdirectory(src/core)   # 核心库
add_subdirectory(src)        # 主程序

#----------------------------------------
# 测试模块
#----------------------------------------
if(UNIT_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

#----------------------------------------
# 安装配置
#----------------------------------------
include(cmake/InstallConfig.cmake)

#----------------------------------------
# 编译加速
#----------------------------------------
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

