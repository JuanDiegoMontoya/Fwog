add_subdirectory(external)

add_custom_target(copy_shaders ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)
add_custom_target(copy_models ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models)

add_executable(hello_triangle hello_triangle.cpp common.cpp common.h)
target_link_libraries(hello_triangle PRIVATE glfw lib_glad lib_gsdf)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

add_executable(deferred deferred.cpp common.cpp common.h)
target_link_libraries(deferred PRIVATE glfw lib_glad lib_gsdf glm)
add_dependencies(deferred copy_shaders)

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        83bb1a48d79729f2688d9c63e2754ce976a65f72
)
FetchContent_GetProperties(tinygltf)

add_executable(gltf_viewer gltf_viewer.cpp common.cpp common.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(gltf_viewer PRIVATE ${tinygltf_SOURCE_DIR})
target_link_libraries(gltf_viewer PRIVATE glfw lib_glad lib_gsdf glm)
add_dependencies(gltf_viewer copy_shaders copy_models tinygltf)