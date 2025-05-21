cmake_minimum_required(VERSION 3.16)

# Define project
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/main.cpp
    src/input.cpp
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
