cmake_minimum_required(VERSION 3.6)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(tinycore)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_SKIP_BUILD_RPATH 0)
set(CMAKE_BUILD_WITH_INSTALL_RPATH 0)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH 1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/macros")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

include(CheckCXXSourceRuns)
include(CheckIncludeFiles)

include(CheckPlatform)
include(AutoCollect)
set(CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS}")
#set(CMAKE_CXX_FLAGS "-fsanitize=address -O1  -fno-omit-frame-pointer -Wall ${CMAKE_CXX_FLAGS}")
#add_compile_options(-Wall -fsanitize=address -O1  -fno-omit-frame-pointer)
add_subdirectory(dep)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(example)
