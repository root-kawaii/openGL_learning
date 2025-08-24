cmake_minimum_required(VERSION 3.16)

# Define project
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------- Debug Configuration ---------
# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra -DDEBUG")

# Release flags (for reference)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Enable debug symbols for all configurations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# Generate compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------- Paths to dependencies ---------
# GLFW
# set(GLFW_DIR "C:/Users/teore/scoop/apps/glfw/current")
# set(GLFW_LIB_DIR "${GLFW_DIR}/lib-mingw-w64")
# set(GLFW_INCLUDE_DIR "${GLFW_DIR}/include")

# ASSIMP
set(ASSIMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/assimp")
set(ASSIMP_LIB_DIR "${ASSIMP_DIR}/lib")
set(ASSIMP_INCLUDE_DIR "${ASSIMP_DIR}/include")

# FreeType
set(FREETYPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/freetype")
set(FREETYPE_INCLUDE_DIRS "${FREETYPE_DIR}/include/freetype2")
set(FREETYPE_LIBRARIES "${FREETYPE_DIR}/lib/libfreetype.a") # adjust if it's .lib or .dll.a

# ImGui - Set up paths
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")
set(IMGUI_INCLUDE_DIR "${IMGUI_DIR}")

# Tracy
set(TRACY_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracy/public")

# --------- ImGui Files ---------
# Collect ImGui source files
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
)

# Collect ImGui header files
file(GLOB IMGUI_HEADERS
    "${IMGUI_DIR}/*.h"
)

# Selectively include only the ImGui backends you need
set(IMGUI_BACKENDS
    # Only include backends for libraries you actually use
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.h"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.h"
    # Add other backends only if you need them:
    # "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    # "${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
)

# Check if backend files exist before adding them
set(IMGUI_EXISTING_BACKENDS)
foreach(backend_file ${IMGUI_BACKENDS})
    if(EXISTS ${backend_file})
        list(APPEND IMGUI_EXISTING_BACKENDS ${backend_file})
    endif()
endforeach()

# Combine all ImGui files
set(IMGUI_ALL_FILES ${IMGUI_SOURCES} ${IMGUI_HEADERS} ${IMGUI_EXISTING_BACKENDS})

# --------- Find packages ---------
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenAL REQUIRED)

# --------- Include directories ---------
include_directories(
    ${IMGUI_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TRACY_PUBLIC_DIR}
)

# --------- Main project source files ---------
set(PROJECT_SOURCES
    src/texture_debugger.cpp
    src/main.cpp
    src/input.cpp
    src/model.cpp
    src/raycast.cpp
    src/game_object.cpp
    src/glad.c
    src/stb_image.cpp
    src/scene.cpp
    src/game.cpp
    src/render_manager.cpp
    src/texture.cpp
    src/audio_manager.cpp
    src/sphere_collision.cpp
)

# --------- ImGui backend files (only the ones you need) ---------
set(IMGUI_BACKEND_SOURCES
    # Add these only if you're using ImGui with OpenGL + GLFW
    include/imgui/backends/imgui_impl_opengl3.cpp
    include/imgui/backends/imgui_impl_glfw.cpp
)

# --------- Add executable ---------
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${IMGUI_SOURCES}
    ${IMGUI_HEADERS}
    ${IMGUI_BACKEND_SOURCES}
    ${TRACY_PUBLIC_DIR}/TracyClient.cpp
)

# --------- Link directories ---------
link_directories(
    ${GLFW_LIB_DIR}
    ${ASSIMP_LIB_DIR}
)

# --------- Link libraries ---------
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw # Provided by your local libglfw3.a
    ${FREETYPE_LIBRARIES}
    OpenAL::OpenAL
    assimp # Must match the lib file (e.g. libassimp.dll.a or assimp.lib)
)


target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)