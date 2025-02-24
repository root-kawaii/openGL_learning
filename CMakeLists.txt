cmake_minimum_required(VERSION 3.16)
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AddressSanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

find_package(OpenGL REQUIRED)

# Find FreeType
find_package(Freetype REQUIRED)
# Find and link GLFW
find_package(glfw3 REQUIRED)
# find_package(OpenGL REQUIRED)

# Add ImGui (if it's inside "external/imgui")
set(IMGUI_DIR include/imgui)
set(ASSIMP_DIR include/assimp)
set(ASSIMP_DIR_BIN /Users/root-kawaii/ComputerGraphics/Vulkan/hephaestus/openGL/include/assimp/bin)

# file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cpp ${SRC_DIR}/*.c)
# file(GLOB_RECURSE HEADER_FILES ${SRC_DIR}/*.h ${SRC_DIR}/*.hpp)

# Add executable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/glad.c 
    src/resource_manager.cpp
    src/game.cpp
    src/shader.cpp
    src/texture.cpp
    src/sprite_renderer.cpp
    src/game_level.cpp
    src/game_object.cpp
    src/main_char.cpp
    src/text_renderer.cpp
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
    src/resource_manager.h
    src/model.h
    src/game.h
    src/shader.h
    src/texture.h
    src/sprite_renderer.h
    src/game_level.h
    src/game_object.h
    src/main_char.h
    src/text_renderer.h
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ASSIMP_DIR}
    ${ASSIMP_DIR_BIN}
    ${FREETYPE_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL "-framework OpenGL")

target_link_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_DIR_BIN})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ASSIMP_DIR_BIN}/libassimp.dylib)
# Link FreeType library
target_link_libraries(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARIES})


