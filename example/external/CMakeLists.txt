# external content definitions
include(FetchContent)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.2
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        cc98465e3508535ba8c7f6208df934c156a018dc
)

option(TINYGLTF_BUILD_LOADER_EXAMPLE "" OFF)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        544969b7324cd6bba29f6203c7d78c7ea92dbab0
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.88
)

if(FWOG_FSR2_ENABLE)
    option(GFX_API_DX12 "" OFF)
    option(GFX_API_VK "" OFF)
    option(GFX_API_GL "" ON)
    option(FFX_FSR2_API_DX12 "" OFF)
    option(FFX_FSR2_API_VK "" OFF)
    option(FFX_FSR2_API_GL "" ON)
    FetchContent_Declare(
        fsr2
        GIT_REPOSITORY https://github.com/JuanDiegoMontoya/FidelityFX-FSR2.git
        GIT_TAG        c894360c183c94e12ee1b520193dc0f906f21502
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(fsr2)
endif()

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(tinygltf)

set(FSR2_SOURCE ${fsr2_SOURCE_DIR} PARENT_SCOPE)

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

FetchContent_MakeAvailable(glfw)

target_compile_definitions(glm INTERFACE GLM_FORCE_SILENT_WARNINGS)
# add_subdirectory(glad)