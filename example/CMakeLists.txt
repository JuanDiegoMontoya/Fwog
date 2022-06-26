include(FetchContent)
add_subdirectory(external)

add_custom_target(copy_shaders ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)
add_custom_target(copy_models ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models)
add_custom_target(copy_textures ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures)

add_executable(hello_triangle hello_triangle.cpp common/common.cpp common/common.h)
target_link_libraries(hello_triangle PRIVATE glfw lib_glad lib_fwog)

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)

#add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.2
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        0.9.9.8
)

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.5.0
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.84.2
)

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(tinygltf)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)

    add_library(lib_imgui
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

    target_include_directories(lib_imgui PUBLIC 
        ${imgui_SOURCE_DIR} 
        ${imgui_SOURCE_DIR}/backends
        ${glfw_SOURCE_DIR}/include)

    target_link_libraries(lib_imgui PRIVATE glfw)
endif()

add_executable(deferred deferred.cpp common/common.cpp common/common.h)
target_link_libraries(deferred PRIVATE glfw lib_glad lib_fwog glm)
add_dependencies(deferred copy_shaders)

add_executable(gltf_viewer gltf_viewer.cpp common/common.cpp common/common.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(gltf_viewer PUBLIC ${tinygltf_SOURCE_DIR})
target_link_libraries(gltf_viewer PRIVATE glfw lib_glad lib_fwog glm)
add_dependencies(gltf_viewer copy_shaders copy_models)

add_executable(volumetric volumetric.cpp common/common.cpp common/common.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(volumetric PUBLIC ${tinygltf_SOURCE_DIR} vendor)
target_link_libraries(volumetric PRIVATE glfw lib_glad lib_fwog glm lib_imgui)
add_dependencies(volumetric copy_shaders copy_models copy_textures)
