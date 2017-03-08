cmake_minimum_required(VERSION 3.6)
project(tinycore)

set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

add_subdirectory(dep)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(example)
