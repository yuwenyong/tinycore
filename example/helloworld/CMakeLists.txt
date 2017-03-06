

add_executable(helloworld helloworld.cpp)
add_dependencies(helloworld tinycore)
target_link_libraries(helloworld tinycore)