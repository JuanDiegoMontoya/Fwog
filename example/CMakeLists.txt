# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.14)

project(exampleProj)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external)

set(source_files
	example.cpp
)

set(header_files
)

add_executable(example ${source_files} ${header_files})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT example)
target_include_directories(example PUBLIC src)

find_package(OpenGL REQUIRED)

# find_package(gsdf REQUIRED)
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../)

# enable asan for debug builds
if (DEBUG)
    if (WIN32)
        target_compile_options(example PUBLIC /fsanitize=address)
    else()
        target_compile_options(example PUBLIC -fsanitize=address)
    endif()
endif()

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" GAME_COMPILER_CLANGPP)
if(GAME_COMPILER_CLANGPP GREATER -1)
  set(GAME_COMPILER_CLANGPP 1)
else()
  set(GAME_COMPILER_CLANGPP 0)
endif()

target_compile_options(example
	INTERFACE
	$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,${GAME_COMPILER_CLANGPP}>:
	-Wall
	-Wextra
	#-pedantic-errors
	-Wconversion
	-Wsign-conversion>
	$<$<CXX_COMPILER_ID:MSVC>:
	#/WX-
	/W3
	>
)

target_link_libraries(example glfw lib_glad lib_gsdf)