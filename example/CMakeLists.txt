add_subdirectory(external)

add_custom_target(copy_shaders ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)
add_custom_target(copy_models ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models)
add_custom_target(copy_textures ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures)

add_executable(01_hello_triangle "01_hello_triangle.cpp" common/Application.cpp common/Application.h)
target_link_libraries(01_hello_triangle PRIVATE glfw lib_glad fwog glm lib_imgui)

add_executable(02_deferred "02_deferred.cpp" common/Application.cpp common/Application.h "common/RsmTechnique.h" "common/RsmTechnique.cpp")
target_include_directories(02_deferred PUBLIC ${tinygltf_SOURCE_DIR} vendor)
target_link_libraries(02_deferred PRIVATE glfw lib_glad fwog glm lib_imgui)
add_dependencies(02_deferred copy_shaders copy_textures)

add_executable(03_gltf_viewer "03_gltf_viewer.cpp" common/Application.cpp common/Application.h common/SceneLoader.cpp common/SceneLoader.h "common/RsmTechnique.h" "common/RsmTechnique.cpp")
target_include_directories(03_gltf_viewer PUBLIC ${tinygltf_SOURCE_DIR} vendor)
target_link_libraries(03_gltf_viewer PRIVATE glfw lib_glad fwog glm lib_imgui)
add_dependencies(03_gltf_viewer copy_shaders copy_models copy_textures)

add_executable(04_volumetric "04_volumetric.cpp" common/common.cpp common/common.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(04_volumetric PUBLIC ${tinygltf_SOURCE_DIR} vendor)
target_link_libraries(04_volumetric PRIVATE glfw lib_glad fwog glm lib_imgui)
add_dependencies(04_volumetric copy_shaders copy_models copy_textures)

add_executable(05_gpu_driven "05_gpu_driven.cpp" common/Application.cpp common/Application.h common/SceneLoader.cpp common/SceneLoader.h)
target_include_directories(05_gpu_driven PUBLIC ${tinygltf_SOURCE_DIR} vendor)
target_link_libraries(05_gpu_driven PRIVATE glfw lib_glad fwog glm lib_imgui)
add_dependencies(05_gpu_driven copy_shaders copy_models)

if (MSVC)
    target_compile_definitions(03_gltf_viewer PUBLIC STBI_MSC_SECURE_CRT)
    target_compile_definitions(04_volumetric PUBLIC STBI_MSC_SECURE_CRT)
    target_compile_definitions(05_gpu_driven PUBLIC STBI_MSC_SECURE_CRT)
endif()