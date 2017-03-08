file(GLOB sources_common common/*.h common/*.cpp)
file(GLOB sources_configuration configuration/*.h configuration/*.cpp)
file(GLOB sources_debugging debugging/*.h debugging/*.cpp)
file(GLOB sources_logging logging/*.h logging/*.cpp)
file(GLOB sources_networking networking/*.h networking/*.cpp)
file(GLOB sources_utilities utilities/*.h utilities/*.cpp)


set(tinycore_srcs
        ${sources_common}
        ${sources_configuration}
        ${sources_debugging}
        ${sources_logging}
        ${sources_networking}
        ${sources_utilities})

link_directories("/usr/local/lib64")
include_directories(${CMAKE_SOURCE_DIR}/src/)

add_library(tinycore SHARED ${tinycore_srcs})
target_link_libraries(tinycore pthread boost_system boost_log)