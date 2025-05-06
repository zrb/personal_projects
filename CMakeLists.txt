cmake_policy(SET CMP0048 NEW)

cmake_minimum_required(VERSION 3.31)

set (CMAKE_CXX_STANDARD 26)
set (CMAKE_GENERATOR "Ninja")
set (CMAKE_CXX_COMPILER g++-15)
set (CMAKE_C_COMPILER gcc-15)
# set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra")
set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -fsanitize=address -std=gnu++26 -fmodules-ts")
set (CMAKE_LINK_FLAGS "-lasan")
# set (CMAKE_CXX_COMPILER clang++-17)
# set (CMAKE_C_COMPILER clang-17)
# set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -fsanitize=address -std=c++23 -stdlib=libc++ -fexperimental-library")
# set (CMAKE_LINK_FLAGS "-lasan -L/usr/lib/llvm-17/lib")
# set (CMAKE_EXE_LINKER_FLAGS "-L/usr/lib/llvm-17/lib")

function(def_shared_library LIB_NAME LIB_VERSION SRCS INCLUDE_PRIVATE INCLUDE_EXPORT DEPS_PRIVATE DEPS_EXPORT)
    project(${LIB_NAME} VERSION ${LIB_VERSION})
    add_library(${LIB_NAME} SHARED)
    target_sources(${LIB_NAME}
        PRIVATE
            ${SRCS}
    )
    include_directories(${INCLUDE})
    target_include_directories(${INCLUDE_EXPORT})
endfunction(def_shared_library LIB_NAME LIB_VERSION)

add_subdirectory(iouring-cpp)
add_subdirectory(logging)
add_subdirectory(reflection)
add_subdirectory(thirdparty)
add_subdirectory(types)
