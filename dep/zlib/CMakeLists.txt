
find_package(ZLIB REQUIRED)
add_library(zlib SHARED IMPORTED GLOBAL)
set_target_properties(zlib
        PROPERTIES
        IMPORTED_LOCATION
        "${ZLIB_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES
        "${ZLIB_INCLUDE_DIRS}")
