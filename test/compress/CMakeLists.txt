add_executable(compress compress.cpp)
add_dependencies(compress tinycore)
target_link_libraries(compress tinycore)