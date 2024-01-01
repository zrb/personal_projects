project(iouring-cpp VERSION 0.0.1)
message(${PROJECT_NAME}-${PROJECT_VERSION})
add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/iouring_net.cpp
        src/iouring_service.cpp
        src/iouring_timer.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_VERSION}/lib)

target_link_libraries(${PROJECT_NAME} LINK_PRIVATE uring)
target_link_libraries(${PROJECT_NAME} PUBLIC types)
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC logging)

add_subdirectory(tests)
add_subdirectory(examples)
