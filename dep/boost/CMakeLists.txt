find_package(Boost 1.55 REQUIRED COMPONENTS system filesystem thread program_options iostreams regex log date_time)

add_library(boost INTERFACE)

target_link_libraries(boost
        INTERFACE
        ${Boost_LIBRARIES})

target_include_directories(boost
        INTERFACE
        ${Boost_INCLUDE_DIRS})