add_subdirectory(external)

add_custom_target(copy_shaders ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)

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