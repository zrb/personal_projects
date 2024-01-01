include(ExternalProject)

set(THIRDPARTY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/src)
set(THIRDPARTY_INSTALL_DIR ${CMAKE_BINARY_DIR}/thirdparty/install)
set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM 1)
include(catch2.cmake)