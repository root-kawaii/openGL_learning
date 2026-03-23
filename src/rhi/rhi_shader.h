#pragma once

#include <string>

// ─── RHIShader ───────────────────────────────────────────────────────────────
//
// Abstract base for a shader program / graphics pipeline.
//
// This is where OpenGL and Vulkan diverge the most:
//
//   OpenGL: A "shader program" is a linked vertex+fragment shader pair.
//     You set uniforms individually with glUniform calls at any time.
//     Pipeline state (blend mode, depth test, etc.) is set globally.
//
//   Vulkan: A "graphics pipeline" bakes EVERYTHING together at creation:
//     shader modules + vertex layout + blend mode + depth test + render pass.
//     Uniforms go through descriptor sets (pre-allocated, pre-bound).
//     Changing any state = binding a different pipeline object.
//
// Because of this fundamental difference, the shader/pipeline abstraction
// is kept minimal for now. Full unification comes in Phase 6 when we port
// the actual material shaders.
//
// For Phase 5, the concrete backends expose their native types directly:
//   - OpenGL: Shader class (existing shader_m.h)
//   - Vulkan: VkPipeline + VkPipelineLayout + VkDescriptorSetLayout

class RHIShader {
public:
    virtual ~RHIShader() = default;
    virtual void bind() = 0;
};
