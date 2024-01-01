file(GLOB echo_server_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(iouring_echo_server ${echo_server_srcs})
#add_dependencies(iouring_echo_server iouring-cpp)
target_link_libraries(iouring_echo_server PRIVATE ${PROJECT_NAME} iouring-cpp)
