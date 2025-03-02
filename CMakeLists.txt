# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.15)

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
    src/Context.cpp
    src/detail/ShaderGLSL.cpp
    src/detail/ShaderSPIRV.cpp
)

set(fwog_header_files
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
    include/Fwog/Config.h
    include/Fwog/Context.h
    include/Fwog/detail/ContextState.h
    include/Fwog/detail/ShaderGLSL.h
    include/Fwog/detail/ShaderSPIRV.h
)

add_library(fwog ${fwog_source_files} ${fwog_header_files})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT fwog)
target_include_directories(fwog PUBLIC include)

find_package(OpenGL REQUIRED)

target_compile_options(fwog
    PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
    -Wall
    -Wextra
    -pedantic-errors
    -Wno-missing-field-initializers
    -Wno-unused-result
    #-Werror
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

option(FWOG_VCC_ENABLE "Use Vulkan Clang Compiler to compile C and C++ shaders. Experimental feature. Requires LLVM 14.x in the path" FALSE)

if (FWOG_VCC_ENABLE)
    include(FetchContent)
    FetchContent_Declare(
        shady
        GIT_REPOSITORY https://github.com/JuanDiegoMontoya/shady.git
        GIT_TAG        58611db72688f7576b2f564db6ae7499f7280bcd
    )
    FetchContent_MakeAvailable(shady)

    target_link_libraries(fwog 
        driver
        shady_emit_common
        shady
    )
    target_compile_definitions(fwog PUBLIC "-DFWOG_VCC_INCLUDE_DIR=\"${shady_BINARY_DIR}/share/vcc/include/\"")
    target_compile_definitions(fwog PUBLIC FWOG_VCC_ENABLE=1)
    target_sources(fwog PRIVATE
        src/detail/ShaderCPP.cpp
        include/Fwog/detail/ShaderCPP.h
    )
else()
    target_compile_definitions(fwog PUBLIC FWOG_VCC_ENABLE=0)
endif()

option(FWOG_FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics)
    endif()
endif()


target_link_libraries(fwog lib_glad)

option(FWOG_BUILD_EXAMPLES "Build the example projects for Fwog." FALSE)
if (${FWOG_BUILD_EXAMPLES})
    add_subdirectory(example)
endif()

option(FWOG_BUILD_DOCS "Build the documentation for Fwog." FALSE)
if (${FWOG_BUILD_DOCS})
    # Add the cmake folder so the FindSphinx module is found
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
    add_subdirectory(docs)
endif()

install(TARGETS fwog
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(DIRECTORY include/ DESTINATION include)