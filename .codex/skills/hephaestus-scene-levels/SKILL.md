---
name: hephaestus-scene-levels
description: Use when editing levels, scene loading, object metadata, named PBR materials, or gameplay conventions encoded in Hephaestus scene data.
---

# Hephaestus Scene Levels

Start here for scene and level work in this repo.

## Read first

1. `docs/SCENE_AND_LEVELS.md`

Then inspect:

- `src/scene.h`
- `src/scene.cpp`
- `src/serialization_utilities.h`
- `src/game_object.h`
- `src/game_entity.h`
- `src/game_entity.cpp`
- `levels/two.json`

## Important conventions

- Serialized JSON `id` is effectively the persistent string identity.
- Runtime numeric `GameObject::ID` is generated separately.
- Object names currently drive gameplay behavior.
- `ball`, `capsule`, `capsule2`, and `Light_<n>` have special meaning.
- Named PBR materials live in the level JSON and objects reference them through `material_name`.

## Guidance

- If you change level format, update serializer code, scene-loading code, and at least one real level file together.
- Preserve or intentionally migrate name-based gameplay conventions.
- Prefer keeping scene data renderer-agnostic; avoid pushing more renderer-specific behavior into `Scene`.
- If a change touches materials, verify both the serialized definition and the renderer upload path.

## Good outcomes

- level files stay readable
- runtime ID behavior stays predictable
- scene loading remains cache-friendly
- future Vulkan-first work can keep using the same scene data model
