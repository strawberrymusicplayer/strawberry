add_library(getopt STATIC getopt.cpp)
target_compile_definitions(getopt PRIVATE -DSTATIC_GETOPT -D_UNICODE)
target_include_directories(getopt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
