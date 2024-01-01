file(GLOB test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(reflection_test ${test_srcs})
# add_dependencies(reflection_test reflection Catch2)
target_link_libraries(reflection_test PRIVATE ${PROJECT_NAME} Catch2::Catch2WithMain)
