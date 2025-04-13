cmake_minimum_required(VERSION 3.20)
project(NorthTests LANGUAGES C)


find_package(cmocka REQUIRED)


#=============== 测试目标配置 ===============#
add_executable(north_tests 
    test_ib.c
    test_token.c
    test_north.c
)

target_compile_definitions(north_tests PRIVATE
    CMOCKA_MAIN
)

# 链接选项
target_link_libraries(north_tests PRIVATE
    ${CMAKE_DL_LIBS} 
    $<$<CONFIG:Debug>:-lasan -lubsan>    
    cmocka::cmocka
    pthread
    north_core
)

set_target_properties(north_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/test"
)

#=============== 测试注册 ===============#
add_test(NAME north_unit_tests
    COMMAND ${CMAKE_BINARY_DIR}/test/north_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

