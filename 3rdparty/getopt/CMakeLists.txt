add_library(getopt STATIC getopt.c)
target_include_directories(getopt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
