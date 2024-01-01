file(GLOB test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(types_test ${test_srcs})
# add_dependencies(types_test types Catch2)
target_link_libraries(types_test PRIVATE ${PROJECT_NAME} Catch2::Catch2)
