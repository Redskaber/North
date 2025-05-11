# 📁 cmake/InstallConfig.cmake
# 安装配置
install(TARGETS north_core
    EXPORT NorthTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# 打包配置
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/Config.cmake.in
    ${CMAKE_BINARY_DIR}/NorthConfig.cmake
    INSTALL_DESTINATION lib/cmake/North
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/NorthConfigVersion.cmake
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


