# 📁 src/CMakeLists.txt

# CMAKE_C_FLAGS 设置编译选项(禁止优化)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")

# 主程序定义
add_executable(north north.c)
target_link_libraries(north PRIVATE north_core)

# 安装目标
install(TARGETS north
    RUNTIME DESTINATION bin
)

