# 📁 cmake/Dependencies.cmake
# 查找依赖包
find_package(LLVM 17 REQUIRED COMPONENTS core support analysis)
find_package(LibEdit REQUIRED)
find_package(CURL REQUIRED)

