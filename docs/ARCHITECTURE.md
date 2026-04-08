# Hephaestus Architecture Snapshot

This document is a fast orientation guide for future work in this repo.
It describes the codebase as it exists during the OpenGL-to-Vulkan migration,
not as a target architecture.

## Current Snapshot

- The project still boots through an OpenGL-first `Game` setup.
- Vulkan is integrated as a second renderer path from [`src/main.cpp`](../src/main.cpp).
- Most gameplay, editor logic, and scene ownership still assume the older OpenGL stack.
- The Vulkan renderer already renders the same scene data, but it is not yet the engine's primary runtime boundary.

## High-Level Ownership

### `Game`

`Game` is the top-level runtime object and still owns most core systems:

- `RenderManager`
- `AudioManager`
- `InputManager`
- `GameManager`
- `UIManager`
- `VNManager`
- the active `Scene`

Relevant files:

- [`src/game.h`](../src/game.h)
- [`src/game.cpp`](../src/game.cpp)

Important detail:

- `Game::initWindow()` creates the GLFW OpenGL window, initializes GLAD, and initializes the OpenGL ImGui backend before Vulkan is even attempted.

### `main.cpp`

[`src/main.cpp`](../src/main.cpp) is the real integration hub right now.

It does all of the following:

- constructs `Game`
- wires `LevelEditor`, `UIManager`, `RenderManager`, and `Scene`
- initializes OpenGL rendering resources
- tries to create `VulkanContext`
- tries to create `VulkanRenderer`
- chooses between OpenGL and Vulkan every frame
- duplicates some editor/UI/picking logic for the Vulkan path

This file currently acts as the migration seam between the legacy engine flow and the new Vulkan renderer.

### `Scene`

`Scene` is one of the better reusable foundations in the repo.

It owns:

- `GameObject` instances
- `GameEntity` wrappers for gameplay actors
- light data
- object selection / editor state
- history / undo data
- level serialization flow

Relevant files:

- [`src/scene.h`](../src/scene.h)
- [`src/scene.cpp`](../src/scene.cpp)
- [`src/serialization_utilities.h`](../src/serialization_utilities.h)

Important detail:

- `Scene::buildFromSerializer()` loads level JSON, starts one async Assimp importer per unique model path, then builds shared `Model` instances on the main thread.

### `GameObject` and `GameEntity`

`GameObject` holds transform, model, shader/material naming, color, collision radius, and a few gameplay-related flags.

`GameEntity` wraps a `GameObject` when an object participates in gameplay.

Relevant files:

- [`src/game_object.h`](../src/game_object.h)
- [`src/game_entity.h`](../src/game_entity.h)
- [`src/game_entity.cpp`](../src/game_entity.cpp)

### `Model` and `Mesh`

These are the main shared asset/runtime data structures.

- `Model` owns Assimp scene data, animation state, and a list of `Mesh` objects.
- `Mesh` stores CPU vertex/index data and immediately creates OpenGL GPU buffers through `GLMeshData`.

Relevant files:

- [`src/model.h`](../src/model.h)
- [`src/model.cpp`](../src/model.cpp)
- [`src/mesh.h`](../src/mesh.h)
- [`src/rhi/opengl/gl_mesh_data.h`](../src/rhi/opengl/gl_mesh_data.h)
- [`src/rhi/vulkan/vk_mesh_data.h`](../src/rhi/vulkan/vk_mesh_data.h)

Important detail:

- The current "RHI" only covers mesh-buffer upload concepts.
- `Mesh` is still OpenGL-biased because it eagerly allocates GL buffers during construction.
- Vulkan does not share those GL buffers; it uploads the same CPU mesh data again into Vulkan buffers.

### `RenderManager` (legacy but still central)

`RenderManager` is still the main OpenGL renderer and a major "god object".

It currently handles:

- OpenGL render passes
- shader management
- PBR material texture uploads
- framebuffers and ID picking buffers
- terrain, water, grass, skybox, rain, and debug rendering
- animation preparation scheduling
- some editor-facing utility rendering

Relevant files:

- [`src/render_manager.h`](../src/render_manager.h)
- [`src/render_manager.cpp`](../src/render_manager.cpp)

Practical interpretation:

- If a task is "maintain the current game", `RenderManager` still matters a lot.
- If a task is "build the future Vulkan game", `RenderManager` is more reference material than destination architecture.

### `VulkanContext` and `VulkanRenderer`

These files contain the strongest forward-looking engine work.

`VulkanContext` owns:

- Vulkan instance
- debug messenger
- surface
- device
- queues
- swapchain

`VulkanRenderer` owns:

- Vulkan GPU resources for uploaded scene models
- render/depth/MSAA/framebuffer resources
- ImGui Vulkan backend
- shadow, skybox, ID picking, PBR, grass, water, pixel-art, UI text, and ground-plane pipelines

Relevant files:

- [`src/vulkan/vk_context.h`](../src/vulkan/vk_context.h)
- [`src/vulkan/vk_context.cpp`](../src/vulkan/vk_context.cpp)
- [`src/vulkan/vk_renderer.h`](../src/vulkan/vk_renderer.h)
- [`src/vulkan/vk_renderer.cpp`](../src/vulkan/vk_renderer.cpp)
- [`src/vulkan/vk_pipeline.cpp`](../src/vulkan/vk_pipeline.cpp)

Important detail:

- Vulkan already consumes `Scene` and `Model` data directly.
- Vulkan maintains its own GPU-side model cache and per-instance animation buffers.
- The Vulkan path is renderer-rich, but app orchestration still lives outside it.

## Frame Flow Today

At a high level, each frame currently does this:

1. `main.cpp` computes camera matrices.
2. OpenGL ImGui frame starts unconditionally.
3. If OpenGL is active, scene input and gizmos run through OpenGL systems.
4. If Vulkan is active, `VulkanRenderer::drawFrame()` renders the scene and its own ImGui content.
5. If OpenGL is active, `RenderManager` runs shadow/main/rain/grid/UI passes.
6. `game->update()` runs after rendering logic in the outer loop.
7. OpenGL ImGui draw data is rendered unless Vulkan is sharing the same window.

This means the app loop is functional, but not cleanly layered.

## Migration State

### What is already reusable

- level JSON format and serializer
- scene/object/entity data model
- Assimp model import path
- animation runtime in `Model`
- Vulkan scene upload path
- Vulkan pipeline/resource code

### What is still transitional

- `main.cpp` dual-renderer orchestration
- OpenGL-first window/context creation
- OpenGL ImGui as the default app UI path
- `RenderManager` as both renderer and utility bucket
- renderer knowledge leaking into scene/editor flow

### What is still only lightly abstracted

- RHI abstraction
- material system
- frame submission model
- renderer-independent scene submission

## Existing Docs Worth Reusing

- [`ANIMATION_SYSTEM.md`](../ANIMATION_SYSTEM.md)
- [`TRACY_PROFILING_GUIDE.md`](../TRACY_PROFILING_GUIDE.md)
- [`TERRAIN_IMPROVEMENTS.md`](../TERRAIN_IMPROVEMENTS.md)

## Recommended Reading Order For Future Tasks

For renderer or engine work:

1. [`docs/ARCHITECTURE.md`](./ARCHITECTURE.md)
2. [`docs/VULKAN_REWRITE_PLAYBOOK.md`](./VULKAN_REWRITE_PLAYBOOK.md)
3. [`src/main.cpp`](../src/main.cpp)
4. [`src/vulkan/vk_renderer.h`](../src/vulkan/vk_renderer.h)
5. [`src/vulkan/vk_renderer.cpp`](../src/vulkan/vk_renderer.cpp)
6. [`src/render_manager.h`](../src/render_manager.h)
7. [`src/render_manager.cpp`](../src/render_manager.cpp)

For level or gameplay data work:

1. [`docs/SCENE_AND_LEVELS.md`](./SCENE_AND_LEVELS.md)
2. [`src/scene.h`](../src/scene.h)
3. [`src/scene.cpp`](../src/scene.cpp)
4. [`src/serialization_utilities.h`](../src/serialization_utilities.h)
5. [`levels/two.json`](../levels/two.json)
