# Vulkan Rewrite Playbook

This is a practical recommendation for moving from the current hybrid codebase
to a new Vulkan-first game built on the strongest reusable engine pieces.

## Main Recommendation

Treat the repo as having two different layers of value:

- a reusable data/asset foundation
- a temporary application/runtime shell

The foundation is worth preserving.
The shell is probably worth replacing.

## Keep, Wrap, Rewrite

### Keep

These parts look like strong candidates for reuse in a new Vulkan game:

- `SerializationUtilities` and the level JSON format
- `Scene` as the source of world objects, lights, and material references
- `GameObject` / `GameEntity` concepts, if the new game still wants them
- `Model` animation/import logic
- Assimp-based model loading and model sharing
- `VulkanContext`
- `VulkanRenderer` resource and pipeline code
- existing Vulkan shader directory structure

### Wrap or decouple

These parts are useful, but should become less coupled before they become the new engine core:

- `Scene` depending on `RenderManager`
- `GameObject` carrying renderer-specific shader naming
- `Mesh` eagerly creating GL buffers during construction
- UI code depending on OpenGL-style coordinate assumptions
- editor/picking logic living in the runtime loop instead of services/tools

### Rewrite

These parts are the best rewrite targets if the goal is a clean Vulkan-first game:

- [`src/main.cpp`](../src/main.cpp)
- the dual OpenGL/Vulkan frame loop
- the OpenGL-first startup path in `Game`
- most of `RenderManager` as the central rendering abstraction
- duplicated editor/UI logic between OpenGL and Vulkan

## Why This Direction Makes Sense

Right now the Vulkan path already proves something important:

- the scene data model is good enough for Vulkan
- the asset pipeline is good enough for Vulkan
- the renderer can already own its own GPU resources

What is still fighting you is not scene data. It is orchestration.

The biggest source of complexity is that the runtime still behaves like an
OpenGL application that happens to also know how to draw with Vulkan.

For a fresh game, it is cleaner to invert that:

- make the app a Vulkan application
- port only the gameplay/editor pieces you still want
- leave the OpenGL renderer as legacy reference code

## Suggested Target Architecture

### 1. Split app/runtime from rendering backend

Introduce a thin runtime shell whose responsibilities are only:

- create window/input services
- own the active scene/world
- own editor/debug overlays
- collect frame input
- call the renderer with a backend-agnostic frame description

Good sign:

- `main.cpp` becomes small.

### 2. Make scene data mostly renderer-agnostic

The scene layer should be able to exist without directly knowing about:

- `RenderManager`
- OpenGL ID buffers
- renderer-specific picking pipelines

Scene selection and editing are still fine, but they should depend on abstract services rather than concrete OpenGL code.

### 3. Stop `Mesh` from being implicitly OpenGL-owned

This is one of the most important cleanup steps.

Today:

- constructing a `Mesh` allocates GL buffers immediately

Target:

- `Mesh` owns CPU mesh data only
- each renderer/backend uploads its own GPU version lazily

That change makes the asset layer truly reusable.

### 4. Define a Vulkan-first frame contract

Move toward something like:

- scene objects
- camera matrices
- lighting data
- debug draw requests
- UI/text draw requests

being passed into Vulkan through a clear per-frame structure instead of many ad hoc calls from `main.cpp`.

### 5. Move tool/editor code behind services

Level editing, object picking, gizmos, and UI can stay, but they should be treated as systems layered on top of the renderer instead of mixed into the core render loop.

## Proposed Rewrite Sequence

### Phase 1: Freeze the legacy boundary

- Do not deepen the OpenGL/Vulkan toggle system unless a bug fix requires it.
- Treat `RenderManager` as maintenance-only.
- Keep future feature work on the Vulkan side when possible.

### Phase 2: Extract reusable engine data

- Make `Mesh` CPU-data-only.
- Keep `Model` import/animation, but remove assumptions that GL buffers always exist.
- Clarify a renderer-independent material description.

### Phase 3: Introduce a new Vulkan runtime shell

- New app entry point
- New frame loop
- Vulkan-only startup path
- Clean ownership of input, scene, and overlays

### Phase 4: Port only the gameplay you still want

Do not drag everything forward automatically.

Port intentionally:

- camera/input behavior you still like
- scene loading
- object/entity concepts you still want
- UI/editor features that still fit the new game

Leave behind:

- old OpenGL rendering passes
- GL-specific debug buffers
- duplicate code paths created only for migration

### Phase 5: Build the new game on top

Once the shell is clean, start the new game logic there instead of inside the old hybrid runtime.

## Practical “Keep vs Scrap” Guidance

### Strong keep candidates

- `src/serialization_utilities.h`
- `src/scene.h`
- `src/scene.cpp`
- `src/model.h`
- `src/model.cpp`
- `src/vulkan/`
- `shaders/vulkan/`

### Likely scrap-or-replace candidates

- most of the top-level flow in `src/main.cpp`
- most of the OpenGL-centric portions of `src/render_manager.cpp`
- any code whose main job is synchronizing OpenGL UI/editor behavior with the Vulkan path

### Conditional keep candidates

These depend on what the new game actually is:

- `GameEntity`
- turn-based movement logic
- `GameManager`
- `VNManager`
- current `UIManager`

If the new game is fundamentally different, these may be cheaper to rewrite than to preserve.

## Known Traps During Rewrite

### Trap 1: keeping both renderers as first-class forever

That usually keeps the old architecture alive longer than intended.

### Trap 2: pushing more responsibilities into `RenderManager`

That helps short-term shipping, but makes the future engine harder to simplify.

### Trap 3: assuming the current RHI is already enough

It is useful, but still very thin and very mesh-focused.
Do not assume it already gives you a clean engine boundary.

### Trap 4: forgetting that names encode gameplay semantics

Some scene/game behavior currently depends on object names like `ball`, `capsule`, and `capsule2`.
If you preserve levels, preserve or intentionally migrate those conventions.

## Decision Shortcut For Future Tasks

When touching code, ask:

1. Is this helping the current hybrid app limp forward?
2. Or is this making the future Vulkan-first engine cleaner?

If the answer is only the first one, prefer the smallest safe change.
If the answer is the second one, that work is usually worth investing in.
