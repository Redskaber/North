cmake_minimum_required(VERSION 3.20)
project(
    North 
    LANGUAGES C
    VERSION 0.1.0 
    DESCRIPTION "A base-c impl self compiler instance."
)

# 编译选项
set(CMAKE_C_STANDARD 17)
set(LLVM_DIR "/usr/lib/llvm-17/lib/cmake/llvm")

# 目录结构
set(PROJECT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECT_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 主程序源文件（排除测试文件）
file(GLOB_RECURSE MAIN_SOURCES
    "${PROJECT_SRC_DIR}/*.c"
    EXCLUDE
    "${PROJECT_SRC_DIR}/test/*.c"
)


# 主可执行目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})

# 头文件包含路径: 只能在可执对象之后时有效
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_INC_DIR}
    # ${PROJECT_SRC_DIR}
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LLVM_LIBS}
    ${Zstd_LIBRARIES}
    ${LibEdit_LIBRARIES}
    ${CURL_LIBRARIES}
    atomic
    ffi
    curses
    tinfo
    xml2
    pthread  # 确保POSIX线程支持

)


target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall              # 启用所有警告
    -Werror            # 将警告转为错误
    -Wextra            # 启用所有扩展警告
    -O3                # 最优化
    -mcx16             # 启用 CMPXCHG16B 指令
    -march=native      # 针对本地 CPU 优化
)



# 测试配置
option(UNIT_TESTING "Enable unit tests" OFF)
if(UNIT_TESTING)
    # 测试源文件
    file(GLOB TEST_SOURCES
        test/token/*.c
        src/token.c
    )

    # 测试可执行目标
    add_executable(${PROJECT_NAME}_token_test ${TEST_SOURCES})

    # 在 add_executable 之后添加：
    set_target_properties(${PROJECT_NAME}_token_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )

    target_include_directories(${PROJECT_NAME}_token_test PRIVATE
        ${PROJECT_INC_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    target_compile_definitions(${PROJECT_NAME}_token_test PRIVATE
        UNIT_TESTING=1
    )

    target_link_libraries(${PROJECT_NAME}_token_test PRIVATE
        cmocka
        ${Zstd_LIBRARIES}
        ${LibEdit_LIBRARIES}
        atomic
    )

    enable_testing()
    add_test(NAME ${PROJECT_NAME}_token_test 
        COMMAND $<TARGET_FILE:${PROJECT_NAME}_token_test>)
endif()                   

# CCache加速配置
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)


# # 清理旧构建
# rm -rf build && mkdir build && cd build

# # 配置项目（启用单元测试）
# cmake -G Ninja -DUNIT_TESTING=ON -DCMAKE_BUILD_TYPE=Debug ..
# # 显式构建测试目标
# ninja North_token_test
# # 验证可执行文件存在
# ls -l bin/North_token_test

# # 方式一：直接运行测试程序
# ./bin/North_token_test
# # 方式二：通过ctest运行（需指定测试名）
# ctest -R North_token_test --output-on-failure -V

