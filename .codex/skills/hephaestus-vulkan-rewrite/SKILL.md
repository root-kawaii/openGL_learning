---
name: hephaestus-vulkan-rewrite
description: Use when working on this repo's Vulkan migration, renderer architecture, engine extraction, or plans to replace the OpenGL-first runtime with a Vulkan-first game shell.
---

# Hephaestus Vulkan Rewrite

Start here for renderer or engine work in this repo.

## Read first

1. `docs/ARCHITECTURE.md`
2. `docs/VULKAN_REWRITE_PLAYBOOK.md`

Read these files next when the task is implementation-heavy:

- `src/main.cpp`
- `src/game.h`
- `src/game.cpp`
- `src/render_manager.h`
- `src/render_manager.cpp`
- `src/vulkan/vk_context.h`
- `src/vulkan/vk_context.cpp`
- `src/vulkan/vk_renderer.h`
- `src/vulkan/vk_renderer.cpp`
- `src/model.h`
- `src/mesh.h`

## Working assumptions

- The repo is still OpenGL-first at startup.
- Vulkan already has substantial rendering capability, but app orchestration still lives in `main.cpp`.
- `RenderManager` is still the legacy rendering center of gravity.
- `Scene`, `Model`, and the level format are stronger long-term assets than the current top-level runtime loop.

## Guidance

- Prefer changes that make the future Vulkan-first architecture cleaner.
- Avoid deepening the dual-renderer toggle path unless the task is explicitly legacy maintenance.
- Treat `RenderManager` as a legacy dependency to contain, not the ideal destination for new engine abstractions.
- Remember that Vulkan uploads its own GPU resources from the same CPU mesh/model data; GL and Vulkan resources are separate.
- Be careful around `Mesh`: constructing it still creates OpenGL buffers immediately.

## When the task touches specific areas

- Animation/model import: also read `ANIMATION_SYSTEM.md`.
- Performance/profiling: also read `TRACY_PROFILING_GUIDE.md`.
- Terrain rendering: also read `TERRAIN_IMPROVEMENTS.md`.

## Good outcomes

- smaller `main.cpp`
- less renderer knowledge in scene/gameplay code
- clearer Vulkan-first ownership
- reusable CPU-side asset/scene data
