
add_executable(chatroom chatroom.cpp)
add_dependencies(chatroom tinycore)
target_link_libraries(chatroom tinycore)