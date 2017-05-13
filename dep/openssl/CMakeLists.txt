

set(_OPENSSL_ROOT_PATHS "/usr/local/ssl")

find_path(OPENSSL_ROOT_DIR
        NAMES
        include/openssl/ssl.h
        PATHS
        ${_OPENSSL_ROOT_PATHS}
        )

find_package(OpenSSL REQUIRED)

add_library(openssl INTERFACE)

target_link_libraries(openssl
        INTERFACE
        ${OPENSSL_LIBRARIES})

target_include_directories(openssl
        INTERFACE
        ${OPENSSL_INCLUDE_DIR})

message(STATUS ${OPENSSL_INCLUDE_DIR})
message(STATUS ${OPENSSL_LIBRARIES})