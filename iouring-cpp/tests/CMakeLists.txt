file(GLOB test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(iouring_test ${test_srcs})
# add_dependencies(iouring_test iouring-cpp Catch2)
target_link_libraries(iouring_test PRIVATE ${PROJECT_NAME} Catch2::Catch2)
