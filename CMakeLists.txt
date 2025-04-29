cmake_minimum_required(VERSION 3.16)

# Define project
project(hephaestus-openGL)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set library paths
set(GLFW_DIR "C:/Users/teore/scoop/apps/glfw/current")
set(ASSIMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/assimp")

find_package(OpenGL REQUIRED)

# find_package(Freetype REQUIRED)
set(FREETYPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/freetype") # Using your previous path
set(FREETYPE_INCLUDE_DIRS "${FREETYPE_DIR}/include/freetype2")
set(FREETYPE_LIBRARIES "${FREETYPE_DIR}/lib/libfreetype.a") # Adjust filename as needed




# Include directories
include_directories("${GLFW_DIR}/include")
include_directories("${ASSIMP_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/assimp")


# Add executable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/input.cpp 
    src/object.cpp 
    src/glad.c 
    # src/resource_manager.cpp
    # src/game.cpp
    # src/shader.cpp
    # src/texture.cpp
    # src/sprite_renderer.cpp
    # src/game_level.cpp
    # src/game_object.cpp
    # src/main_char.cpp
    # src/text_renderer.cpp
    src/stb_image.cpp
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/backends/imgui_impl_glfw.cpp
    include/imgui/backends/imgui_impl_opengl3.cpp
)

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE 
    include 
    # src/resource_manager.h
    src/model.h
    # src/game.h
    # src/shader.h
    # src/texture.h
    # src/sprite_renderer.h
    # src/game_level.h
    # src/game_object.h
    # src/main_char.h
    # src/text_renderer.h
    src/stb_image.h
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${FREETYPE_INCLUDE_DIRS}

)

# Link directories (needed for libraries)
link_directories("${GLFW_DIR}/lib-mingw-w64")
link_directories("${ASSIMP_DIR}/lib")


# Link libraries correctly
target_link_libraries(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/assimp/lib/libassimp.dll.a"
    # "${ASSIMP_DIR}/lib"
    ${FREETYPE_LIBRARIES}
    "${GLFW_DIR}/lib-mingw-w64/libglfw3.a"  # Use the static library, not the DLL
    OpenGL::GL
)

# Link libraries for Windows
# target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL)
# target_link_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_DIR_BIN})
# target_link_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_DIR})
# link_directories("${GLFW_DIR}/lib-mingw-w64")
