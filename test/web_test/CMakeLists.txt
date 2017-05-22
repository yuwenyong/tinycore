add_executable(web_test web_test.cpp)
add_dependencies(web_test tinycore)
target_link_libraries(web_test tinycore)