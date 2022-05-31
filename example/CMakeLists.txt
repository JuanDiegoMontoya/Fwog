add_subdirectory(external)

add_custom_target(copy_shaders ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)
add_custom_target(copy_models ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models)

add_executable(hello_triangle hello_triangle.cpp common/common.cpp common/common.h)
target_link_libraries(hello_triangle PRIVATE glfw lib_glad lib_fwog)

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
FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(tinygltf)

add_executable(deferred deferred.cpp common/common.cpp common/common.h)
target_link_libraries(deferred PRIVATE glfw lib_glad lib_fwog glm)
add_dependencies(deferred copy_shaders)

add_executable(gltf_viewer gltf_viewer.cpp common/common.cpp common/common.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(gltf_viewer PUBLIC ${tinygltf_SOURCE_DIR})
target_link_libraries(gltf_viewer PRIVATE glfw lib_glad lib_fwog glm)
add_dependencies(gltf_viewer copy_shaders copy_models)