#version 450

// ── Inputs from vertex shader ───────────────────────────────────────────────
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// ── Output ──────────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Descriptors ─────────────────────────────────────────────────────────────
layout(set = 0, binding = 1) uniform sampler2D diffuseTexture;

// Simple directional light for now — enough to verify the model looks correct
void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 normal   = normalize(fragNormal);

    // Ambient + diffuse
    float ambient = 0.15;
    float diff    = max(dot(normal, lightDir), 0.0);
    float light   = ambient + diff * 0.85;

    vec4 texColor = texture(diffuseTexture, fragTexCoord);
    outColor = vec4(texColor.rgb * light, texColor.a);
}
