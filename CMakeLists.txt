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

# --------- AddressSanitizer Configuration ---------
# Add option to enable sanitizers
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

if(ENABLE_SANITIZERS)
    message(STATUS "Building with AddressSanitizer and UBSan enabled")
    set(SANITIZER_FLAGS "-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

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

# ImGuizmo - Set up paths
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/ImGuizmo")
set(IMGUIZMO_INCLUDE_DIR "${IMGUIZMO_DIR}")

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

# --------- ImGuizmo Files ---------
# Include all ImGuizmo source files
set(IMGUIZMO_SOURCES
    "${IMGUIZMO_DIR}/ImGuizmo.cpp"
    "${IMGUIZMO_DIR}/GraphEditor.cpp"
    "${IMGUIZMO_DIR}/ImCurveEdit.cpp"
    "${IMGUIZMO_DIR}/ImGradient.cpp"
    "${IMGUIZMO_DIR}/ImSequencer.cpp"
)

set(IMGUIZMO_HEADERS
    "${IMGUIZMO_DIR}/ImGuizmo.h"
    "${IMGUIZMO_DIR}/GraphEditor.h"
    "${IMGUIZMO_DIR}/ImCurveEdit.h"
    "${IMGUIZMO_DIR}/ImGradient.h"
    "${IMGUIZMO_DIR}/ImSequencer.h"
    "${IMGUIZMO_DIR}/ImZoomSlider.h"
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
    src/ui.cpp
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
    ${IMGUIZMO_SOURCES}  # Add ImGuizmo sources here
    ${TRACY_PUBLIC_DIR}/TracyClient.cpp
)

# --------- Target-specific include directories (FIXED ORDER) ---------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_INCLUDE_DIR}                    # ImGui MUST come first!
    ${IMGUI_INCLUDE_DIR}/backends           # ImGui backends
    ${IMGUIZMO_DIR}                         # ImGuizmo after ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/include     # Your project includes
    ${ASSIMP_INCLUDE_DIR}                   # Assimp
    ${FREETYPE_INCLUDE_DIRS}                # FreeType
    ${TRACY_PUBLIC_DIR}                     # Tracy
)

# --------- Link directories ---------
link_directories(
    ${GLFW_LIB_DIR}
    ${ASSIMP_LIB_DIR}
)

# --------- Define preprocessor macros ---------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TRACY_ENABLE
    GLM_ENABLE_EXPERIMENTAL          # Enable GLM experimental extensions
    IMGUI_DEFINE_MATH_OPERATORS     # Help ImGuizmo with ImGui types
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Sometimes helps with compatibility
)

# --------- Link libraries ---------
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw # Provided by your local libglfw3.a
    ${FREETYPE_LIBRARIES}
    OpenAL::OpenAL
    assimp # Must match the lib file (e.g. libassimp.dll.a or assimp.lib)
)