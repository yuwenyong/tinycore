
add_executable(logging logging.cpp)
add_dependencies(logging tinycore)
target_link_libraries(logging tinycore)