
add_executable(httpclient_test httpclient_test.cpp)
add_dependencies(httpclient_test tinycore)
target_link_libraries(httpclient_test tinycore)

add_custom_command(TARGET httpclient_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.crt ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.key ${CMAKE_CURRENT_BINARY_DIR}/
        )