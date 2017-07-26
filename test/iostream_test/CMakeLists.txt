
add_executable(iostream_test iostream_test.cpp)
add_dependencies(iostream_test tinycore)
target_link_libraries(iostream_test tinycore)
add_custom_command(TARGET iostream_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.crt ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.key ${CMAKE_CURRENT_BINARY_DIR}/
        )