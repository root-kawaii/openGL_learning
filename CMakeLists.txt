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

# --------- Find packages first (preferred method) ---------
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SNDFILE QUIET sndfile)
endif()
if(NOT SNDFILE_FOUND)
    find_path(SNDFILE_INCLUDE_DIR sndfile.h PATHS /opt/homebrew/include /usr/local/include)
    find_library(SNDFILE_LIBRARY sndfile PATHS /opt/homebrew/lib /usr/local/lib)
endif()

# Try to find FreeType using find_package first
find_package(Freetype QUIET)

# --------- Paths to dependencies ---------
# ASSIMP
set(ASSIMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/assimp")
set(ASSIMP_LIB_DIR "${ASSIMP_DIR}/lib")
set(ASSIMP_INCLUDE_DIR "${ASSIMP_DIR}/include")

# FreeType fallback if find_package didn't work
if(NOT Freetype_FOUND)
    message(STATUS "FreeType not found via find_package, using local build")
    set(FREETYPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/freetype")
    set(FREETYPE_INCLUDE_DIRS "${FREETYPE_DIR}/include/freetype2" "${FREETYPE_DIR}/include")
    
    # Try to find the correct library file
    find_library(FREETYPE_LIBRARY
        NAMES freetype libfreetype
        PATHS "${FREETYPE_DIR}/lib"
        NO_DEFAULT_PATH
    )
    
    if(NOT FREETYPE_LIBRARY)
        set(FREETYPE_LIBRARY "${FREETYPE_DIR}/lib/libfreetype.a")
    endif()
    
    set(FREETYPE_LIBRARIES ${FREETYPE_LIBRARY})
endif()

# ImGui - Set up paths
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")
set(IMGUI_INCLUDE_DIR "${IMGUI_DIR}")

# ImGuizmo - Set up paths
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/ImGuizmo")
set(IMGUIZMO_INCLUDE_DIR "${IMGUIZMO_DIR}")

# Tracy
set(TRACY_PUBLIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracy/public")

# vk-bootstrap
set(VK_BOOTSTRAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/vk-bootstrap/src")

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
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
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

# --------- Main project source files ---------
set(PROJECT_SOURCES
    src/texture_debugger.cpp
    src/main.cpp
    src/input.cpp
    src/model.cpp
    src/raycast.cpp
    src/ascii_level_utils.cpp
    src/game_object.cpp
    src/glad.c
    src/stb_image.cpp
    src/scene.cpp
    src/game.cpp
    src/player.cpp
    src/projectile_bounce_utils.cpp
    src/render_manager.cpp
    src/texture.cpp
    src/audio_clip.cpp
    src/audio_listener_3d.cpp
    src/audio_manager.cpp
    src/audio_source_3d.cpp
    src/sphere_collision.cpp
    src/ui.cpp
    src/level_editor.cpp
    src/game_entity.cpp
    src/game_manager.cpp
    src/vn_manager.cpp
    src/vulkan/vk_context.cpp
    src/vulkan/vk_renderer.cpp
    src/vulkan/vk_pipeline.cpp
    src/vulkan/vk_buffer.cpp
    src/vulkan/vk_texture.cpp
    src/vulkan/vk_mem_alloc_impl.cpp
)

# --------- vk-bootstrap source ---------
set(VK_BOOTSTRAP_SOURCES
    ${VK_BOOTSTRAP_DIR}/VkBootstrap.cpp
)

# --------- ImGui backend files (only the ones you need) ---------
set(IMGUI_BACKEND_SOURCES
    include/imgui/backends/imgui_impl_opengl3.cpp
    include/imgui/backends/imgui_impl_glfw.cpp
    include/imgui/backends/imgui_impl_vulkan.cpp
)

# --------- Add executable ---------
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${IMGUI_SOURCES}
    ${IMGUI_HEADERS}
    ${IMGUI_BACKEND_SOURCES}
    ${IMGUIZMO_SOURCES}  # Add ImGuizmo sources here
    ${VK_BOOTSTRAP_SOURCES}
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
    ${Vulkan_INCLUDE_DIRS}                  # Vulkan
    ${VK_BOOTSTRAP_DIR}                    # vk-bootstrap
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vma # VMA
    ${SNDFILE_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIR}
)

# --------- Link directories ---------
link_directories(
    ${ASSIMP_LIB_DIR}
)

# --------- Define preprocessor macros ---------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TRACY_ENABLE
    GLM_ENABLE_EXPERIMENTAL          # Enable GLM experimental extensions
    IMGUI_DEFINE_MATH_OPERATORS     # Help ImGuizmo with ImGui types
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Sometimes helps with compatibility
)

# --------- Windows-specific linker flags for FreeType ---------
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Fix for MinGW/GCC on Windows with static libraries
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,--allow-multiple-definition
        -Wl,--enable-auto-image-base
        -static-libgcc
        -static-libstdc++
    )
endif()

# --------- Link libraries ---------
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    ${FREETYPE_LIBRARIES}
    OpenAL::OpenAL
    ${SNDFILE_LIBRARIES}
    ${SNDFILE_LIBRARY}
    assimp
    Vulkan::Vulkan
)

# Add Windows-specific system libraries for FreeType
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        # These may be needed for FreeType on Windows
        bz2
        png
        z
        harfbuzz
        # Windows system libraries
        ws2_32
        winmm
        ole32
        oleaut32
        imm32
        version
    )
endif()

# --------- SPIR-V Shader Compilation ---------
# Compile GLSL 450 shaders in shaders/vulkan/ to SPIR-V
set(VULKAN_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan")
set(SPIRV_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan/compiled")

# Create output directory for compiled SPIR-V
file(MAKE_DIRECTORY ${SPIRV_OUTPUT_DIR})

# Find all Vulkan GLSL shaders
file(GLOB_RECURSE VULKAN_SHADERS
    "${VULKAN_SHADER_DIR}/*.vert"
    "${VULKAN_SHADER_DIR}/*.frag"
    "${VULKAN_SHADER_DIR}/*.geom"
    "${VULKAN_SHADER_DIR}/*.comp"
)

# Compile each shader to SPIR-V
foreach(SHADER ${VULKAN_SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV_OUTPUT "${SPIRV_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
    )
    list(APPEND SPIRV_BINARIES ${SPIRV_OUTPUT})
endforeach()

# Custom target to trigger shader compilation
add_custom_target(compile_shaders DEPENDS ${SPIRV_BINARIES})
add_dependencies(${PROJECT_NAME} compile_shaders)

# --------- Print debug information ---------
message(STATUS "FreeType found: ${Freetype_FOUND}")
message(STATUS "FreeType include dirs: ${FREETYPE_INCLUDE_DIRS}")
message(STATUS "FreeType libraries: ${FREETYPE_LIBRARIES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Vulkan found: ${Vulkan_FOUND}")
message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan glslc: ${Vulkan_GLSLC_EXECUTABLE}")
