
add_executable(options_test options_test.cpp)
add_dependencies(options_test tinycore)
target_link_libraries(options_test tinycore)

add_custom_command(TARGET options_test
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/options_test.cfg ${CMAKE_CURRENT_BINARY_DIR}/
        )