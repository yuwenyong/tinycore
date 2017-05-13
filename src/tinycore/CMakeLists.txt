
CollectSourceFiles(${CMAKE_CURRENT_SOURCE_DIR} PRIVATE_SOURCES)
add_library(tinycore SHARED ${PRIVATE_SOURCES})
target_include_directories(tinycore PUBLIC "${CMAKE_SOURCE_DIR}/src/")
target_link_libraries(tinycore PUBLIC openssl boost zlib)

#link_directories("/usr/local/lib")
#include_directories(${CMAKE_SOURCE_DIR}/src/ /usr/local/include)


#target_link_libraries(tinycore pthread openssl z boost)
#target_link_libraries(tinycore pthread ssl z boost_system boost_log boost_thread boost_date_time boost_program_options boost_iostreams)