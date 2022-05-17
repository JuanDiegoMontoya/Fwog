# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.14)

project(fwog)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external)

set(source_files
	src/Buffer.cpp
	src/DebugMarker.cpp
	src/Fence.cpp
	src/Shader.cpp
	src/Texture.cpp
	src/Rendering.cpp
	src/Pipeline.cpp
	src/Timer.cpp
	src/detail/ApiToEnum.cpp
	src/detail/PipelineManager.cpp
	src/detail/FramebufferCache.cpp
	src/detail/SamplerCache.cpp
	src/detail/VertexArrayCache.cpp
)

set(header_files
	include/fwog/Common.h
	include/fwog/BasicTypes.h
	include/fwog/Buffer.h
	include/fwog/DebugMarker.h
	include/fwog/Fence.h
	include/fwog/Indirect.h
	include/fwog/Shader.h
	include/fwog/Texture.h
	include/fwog/Rendering.h
	include/fwog/Pipeline.h
	include/fwog/Timer.h
	include/fwog/detail/Flags.h
	include/fwog/detail/ApiToEnum.h
	include/fwog/detail/PipelineManager.h
	include/fwog/detail/FramebufferCache.h
	include/fwog/detail/Hash.h
	include/fwog/detail/SamplerCache.h
	include/fwog/detail/VertexArrayCache.h
)

add_library(lib_fwog ${source_files} ${header_files})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT lib_fwog)
target_include_directories(lib_fwog PUBLIC include)

find_package(OpenGL REQUIRED)

# enable asan for debug builds
if (DEBUG)
    if (WIN32)
        target_compile_options(lib_fwog PUBLIC /fsanitize=address)
    else()
        target_compile_options(lib_fwog PUBLIC -fsanitize=address)
    endif()
endif()

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" COMPILER_CLANGPP)
if(COMPILER_CLANGPP GREATER -1)
  set(COMPILER_CLANGPP 1)
else()
  set(COMPILER_CLANGPP 0)
endif()

target_compile_options(lib_fwog
	INTERFACE
	$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,${COMPILER_CLANGPP}>:
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

target_link_libraries(lib_fwog lib_glad)

add_subdirectory(example)