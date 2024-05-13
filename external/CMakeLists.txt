# external content definitions
add_library(lib_glad glad/src/gl.c)
target_include_directories(lib_glad PUBLIC glad/include)