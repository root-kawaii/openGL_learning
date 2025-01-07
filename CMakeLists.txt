cmake_minimum_required(VERSION 3.16)
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find and link GLFW
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Add GLFW as a subdirectory (if building from source)
# add_subdirectory(external/glfw)

# Add executable
add_executable(${PROJECT_NAME} src/main.cpp src/glad.c)

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL)
