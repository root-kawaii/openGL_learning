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
set(FREETYPE_LIBRARIES "${FREETYPE_DIR}/lib/libfreetype.a")  # adjust if it's .lib or .dll.a

# --------- Find packages ---------
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# --------- Include directories ---------
include_directories(
    ${GLFW_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------- Add executable ---------
add_executable(${PROJECT_NAME}
    src/texture_debugger.cpp
    src/main.cpp
    src/input.cpp
    src/model.cpp
    src/raycast.cpp
    src/object.cpp
    src/glad.c
    src/stb_image.cpp
)

# --------- Link directories ---------
link_directories(
    ${GLFW_LIB_DIR}
    ${ASSIMP_LIB_DIR}
)

# --------- Link libraries ---------
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw                   # Provided by your local libglfw3.a
    ${FREETYPE_LIBRARIES}
    assimp                  # Must match the lib file (e.g. libassimp.dll.a or assimp.lib)
)
