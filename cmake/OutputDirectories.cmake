# 📁 cmake/OutputDirectories.cmake
# 输出目录配置
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


