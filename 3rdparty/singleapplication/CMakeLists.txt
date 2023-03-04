cmake_minimum_required(VERSION 3.7)

include(CheckIncludeFiles)
include(CheckFunctionExists)

check_function_exists(geteuid HAVE_GETEUID)
check_function_exists(getpwuid HAVE_GETPWUID)

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")

add_subdirectory(singleapplication)
add_subdirectory(singlecoreapplication)
