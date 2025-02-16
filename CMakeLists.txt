cmake_minimum_required(VERSION 3.16)
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find and link GLFW
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Add ImGui (if it's inside "external/imgui")
set(IMGUI_DIR include/imgui)
set(ASSIMP_DIR include/assimp)
set(ASSIMP_DIR_BIN /Users/root-kawaii/ComputerGraphics/Vulkan/hephaestus/openGL/include/assimp/bin)

# Add executable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/glad.c 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp

)

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE 
    include 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ASSIMP_DIR}
    ${ASSIMP_DIR_BIN}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL)

target_link_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_DIR_BIN})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ASSIMP_DIR_BIN}/libassimp.dylib)
