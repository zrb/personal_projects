#[[
include(ExternalProject)

set(CATCH2_SOURCE_DIR ${THIRDPARTY_SOURCE_DIR}/Catch2/v3.4.0)
set(CATCH2_INSTALL_DIR ${THIRDPARTY_INSTALL_DIR}/Catch2/v3.4.0)
set(CATCH2_INCLUDE_DIR ${CATCH2_INSTALL_DIR}/include)

ExternalProject_Add(ExtCatch2
   GIT_REPOSITORY https://github.com/catchorg/Catch2.git
   GIT_TAG v3.4.0
   SOURCE_DIR ${CATCH2_SOURCE_DIR}
   INSTALL_DIR ${CATCH2_INSTALL_DIR}
   CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

ExternalProject_Get_Property(ExtCatch2 install_dir)
message(${install_dir})
include_directories(${install_dir}/include)
list(APPEND CMAKE_PREFIX_PATH ${install_dir})
message(${CMAKE_PREFIX_PATH})
find_package(Catch2)
]]#

include(FetchContent)

set(CATCH2_SOURCE_DIR ${THIRDPARTY_SOURCE_DIR}/Catch2/v3.8.1)
set(CATCH2_INSTALL_DIR ${THIRDPARTY_INSTALL_DIR}/Catch2/v3.8.1)
set(CATCH2_INCLUDE_DIR ${CATCH2_INSTALL_DIR}/include)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR ${CATCH2_SOURCE_DIR}
    INSTALL_DIR ${CATCH2_INSTALL_DIR}
    UPDATE_DISCONNECTED TRUE
)

FetchContent_MakeAvailable(Catch2)
