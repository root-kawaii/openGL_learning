# Scene And Level Notes

This file captures the current world-data workflow so future tasks can edit
levels, materials, scene loading, and gameplay object metadata without
relearning the same details.

## Main Files

- [`src/scene.h`](../src/scene.h)
- [`src/scene.cpp`](../src/scene.cpp)
- [`src/serialization_utilities.h`](../src/serialization_utilities.h)
- [`levels/two.json`](../levels/two.json)

## Level File Structure

The current level JSON has three main sections:

- `materials`
- `objects`
- `lights`

### `materials`

Named PBR material library.

Each entry can contain:

- `albedo`
- `normal`
- `metallic`
- `roughness`
- `ao`
- `displacement`

These are CPU-side file paths.
GPU-side textures are created later by the renderer.

### `objects`

Each object can contain:

- `id`
- `name` (optional in practice)
- `path`
- `shader_name`
- `material_name`
- `position`
- `rotation`
- `scale`
- `color`
- `collision_radius`
- `entity`
- `terrain_type`

Important caveat:

- At load time, JSON `id` is treated more like the object's persistent string name than its runtime numeric ID.

### `lights`

Each light contains:

- `position`
- `color`
- `intensity`

## Scene Build Flow

`Scene::buildFromSerializer()` is the key loader.

It currently does this:

1. Load JSON through `SerializationUtilities`.
2. Copy light data into `sceneLights`.
3. Start one async Assimp import per unique model path.
4. Build shared `Model` instances from those importers.
5. Create light-visualizer `GameObject`s using `assets/torch.glb`.
6. Create one `GameObject` per serialized object.
7. Create `GameEntity` wrappers when the JSON contains `entity`.
8. Upload named PBR materials through the renderer when available.

This means scene loading is already partly optimized for repeated models.

## Runtime Identity Rules

There are two different identities in play.

### String identity

This comes from the serialized JSON `id`.
It becomes the `GameObject` name used by much of the gameplay/editor logic.

Examples:

- `ball`
- `capsule`
- `grid_cube_4_0`

### Numeric runtime identity

`Scene::addGameObject()` generates a fresh numeric `ID` at runtime.
This is the value used by object-picking and in-memory lookup tables.

Practical consequence:

- If you change how objects are saved/loaded, do not assume serialized `id` and runtime `ID` mean the same thing.

## Name-Based Gameplay Conventions

Several gameplay behaviors depend on object names.

Current important conventions:

- object named `ball` becomes `Scene::ball`
- object named `capsule` becomes the startup ball owner when `setStartupState` is true
- object named `capsule` maps to `EntityClassType::STRIKER`
- object named `capsule2` maps to `EntityClassType::DEFENDER`
- runtime-created light visualizers are named `Light_<n>`

If you rename objects or change level conventions, update gameplay assumptions too.

## Material Flow

Named PBR materials live in the level JSON and are stored in `SerializationUtilities`.

Flow:

1. JSON `materials` are parsed into `PBRMaterialDef`.
2. Objects reference them through `material_name`.
3. `Scene::buildFromSerializer()` calls `renderManager->loadPBRMaterials(...)` when a renderer is present.

Practical consequence:

- The material library is already a good engine-level abstraction.
- It would survive a Vulkan-first rewrite well, especially if moved out of `RenderManager`.

## Save Behavior Notes

`SerializationUtilities::saveScene()` currently writes:

- `obj.name` into the serialized `id` field
- `obj.modelPath`
- `obj.shaderName`
- `obj.materialName`
- transform/color/entity/collision data

This matches current usage, but it also reinforces the split between:

- persistent string identity
- generated numeric runtime ID

## Renderer Coupling To Watch

`Scene` is good engine data overall, but it still has renderer coupling:

- `Scene` stores a `RenderManager*`
- scene input and gizmo flow assume current renderer services
- material upload happens through the current renderer

For a new Vulkan-first engine, preserve the scene data model while reducing these couplings.

## Safe Editing Rules For Future Tasks

When changing level format or scene loading:

1. Update `SerializationUtilities` and `Scene` together.
2. Check whether object-name conventions still work.
3. Update a real level file such as [`levels/two.json`](../levels/two.json).
4. If material behavior changes, verify both serialization and renderer upload logic.

## Good Future Refactors

- move material upload out of `Scene`
- separate editor selection/picking from the raw scene container
- make persistent string IDs explicit instead of overloading `name`
- keep runtime numeric IDs as renderer/editor implementation detail
