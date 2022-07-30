# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.14)

project(Fwog)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external)

set(fwog_source_files
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

set(fwog_header_files
	include/Fwog/Common.h
	include/Fwog/BasicTypes.h
	include/Fwog/Buffer.h
	include/Fwog/DebugMarker.h
	include/Fwog/Fence.h
	include/Fwog/Shader.h
	include/Fwog/Texture.h
	include/Fwog/Rendering.h
	include/Fwog/Pipeline.h
	include/Fwog/Timer.h
	include/Fwog/Exception.h
	include/Fwog/detail/Flags.h
	include/Fwog/detail/ApiToEnum.h
	include/Fwog/detail/PipelineManager.h
	include/Fwog/detail/FramebufferCache.h
	include/Fwog/detail/Hash.h
	include/Fwog/detail/SamplerCache.h
	include/Fwog/detail/VertexArrayCache.h
)

add_library(lib_fwog ${fwog_source_files} ${fwog_header_files})

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
	-pedantic-errors
	-Wno-missing-field-initializers
	-Werror
	#-Wconversion
	#-Wsign-conversion
	>
	$<$<CXX_COMPILER_ID:MSVC>:
	/W4
	/WX
	/permissive-
	/wd4324 # structure was padded
	>
)

target_link_libraries(lib_fwog lib_glad)

add_subdirectory(example)
