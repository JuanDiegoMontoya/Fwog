# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.14)

project(gsdf)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external)

set(source_files
	src/Buffer.cpp
	src/DebugMarker.cpp
	src/Fence.cpp
	src/Framebuffer.cpp
	src/Shader.cpp
	src/Texture.cpp
	src/Rendering.cpp
	src/Pipeline.cpp
	src/detail/ApiToEnum.cpp
)

set(header_files
	include/gsdf/Common.h
	include/gsdf/BasicTypes.h
	include/gsdf/Buffer.h
	include/gsdf/DebugMarker.h
	include/gsdf/Fence.h
	include/gsdf/Framebuffer.h
	include/gsdf/Indirect.h
	include/gsdf/Shader.h
	include/gsdf/Texture.h
	include/gsdf/Flags.h
	include/gsdf/Rendering.h
	include/gsdf/Pipeline.h
	include/gsdf/detail/ApiToEnum.h
)

add_library(lib_gsdf ${source_files} ${header_files})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lib_gsdf)
target_include_directories(lib_gsdf PUBLIC include)

find_package(OpenGL REQUIRED)

# enable asan for debug builds
if (DEBUG)
    if (WIN32)
        target_compile_options(lib_gsdf PUBLIC /fsanitize=address)
    else()
        target_compile_options(lib_gsdf PUBLIC -fsanitize=address)
    endif()
endif()

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" GAME_COMPILER_CLANGPP)
if(GAME_COMPILER_CLANGPP GREATER -1)
  set(GAME_COMPILER_CLANGPP 1)
else()
  set(GAME_COMPILER_CLANGPP 0)
endif()

target_compile_options(lib_gsdf
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

target_link_libraries(lib_gsdf lib_glad)

add_subdirectory(example)