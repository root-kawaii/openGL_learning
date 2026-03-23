#pragma once

// ─── RHI Types ───────────────────────────────────────────────────────────────
//
// The Render Hardware Interface (RHI) is an abstraction layer that lets the
// engine use different graphics APIs (OpenGL, Vulkan) through a common interface.
//
// Why an abstraction layer?
//   - OpenGL and Vulkan have fundamentally different designs:
//     OpenGL is a state machine (bind this, set that, draw).
//     Vulkan is explicit (allocate, record commands, submit).
//   - An RHI lets game logic (mesh loading, scene rendering) work without
//     knowing which API is active.
//   - It also lets us migrate incrementally — OpenGL stays working while we
//     build out Vulkan support piece by piece.
//
// Design tradeoffs:
//   - Too thin: just wraps API calls, doesn't save much complexity.
//   - Too thick: hides important details, limits performance.
//   - Our approach: abstract RESOURCE CREATION (buffers, textures) but keep
//     DRAW DISPATCH backend-specific, because GL and VK draw calls are too
//     different to unify cleanly without performance loss.

#include <cstdint>

enum class RHIBackend {
    OpenGL,
    Vulkan
};
