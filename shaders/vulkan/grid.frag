#version 450

layout(location = 0) in  vec2 fragNDC;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;
} frame;

layout(push_constant) uniform GridPush {
    vec4 cameraPos;  // xyz = camera position
} push;

vec4 grid(vec3 worldPos, float scale) {
    vec2 coord = worldPos.xz * scale;
    vec2 deriv = fwidth(coord);
    vec2 g     = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(g.x, g.y);
    float alpha = 1.0 - min(line, 1.0);

    vec4 color = vec4(0.35, 0.35, 0.4, alpha * 0.75);

    // Axis highlights
    if (abs(worldPos.z) < deriv.y * 1.5)
        color = mix(vec4(0.15, 0.15, 1.0, 0.9), color, min(abs(worldPos.z) / (deriv.y * 1.5), 1.0));
    if (abs(worldPos.x) < deriv.x * 1.5)
        color = mix(vec4(1.0, 0.15, 0.15, 0.9), color, min(abs(worldPos.x) / (deriv.x * 1.5), 1.0));

    return color;
}

void main() {
    mat4 invProj = inverse(frame.proj);
    mat4 invView = inverse(frame.view);

    // Ray from camera through this NDC pixel
    vec4 rayClip  = vec4(fragNDC, -1.0, 1.0);
    vec4 rayView4 = invProj * rayClip;
    vec3 rayView  = normalize(rayView4.xyz);
    vec3 rayWorld = normalize(mat3(invView) * rayView);

    vec3 camPos = push.cameraPos.xyz;

    // Intersect with Y = 0 plane
    if (abs(rayWorld.y) < 0.0001) discard;
    float t = -camPos.y / rayWorld.y;
    if (t <= 0.0) discard;

    vec3 gridPos = camPos + t * rayWorld;

    // Distance fade
    float dist = length(gridPos.xz - camPos.xz);
    float fade = 1.0 - smoothstep(80.0, 200.0, dist);
    if (fade <= 0.001) discard;

    vec4 col = grid(gridPos, 1.0);
    col.a *= fade;
    if (col.a < 0.01) discard;

    outColor = col;
}
