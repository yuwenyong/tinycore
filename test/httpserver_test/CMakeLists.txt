add_executable(httpserver_test httpserver_test.cpp)
add_dependencies(httpserver_test tinycore)
message(STATUS ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(httpserver_test tinycore)

add_custom_command(TARGET httpserver_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.crt ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.key ${CMAKE_CURRENT_BINARY_DIR}/
        )