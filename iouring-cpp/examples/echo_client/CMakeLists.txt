file(GLOB echo_client_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(iouring_echo_client ${echo_client_srcs})
#add_dependencies(iouring_echo_client iouring-cpp)
target_link_libraries(iouring_echo_client PRIVATE ${PROJECT_NAME} iouring-cpp)
