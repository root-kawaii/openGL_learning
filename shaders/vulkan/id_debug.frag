#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint objectID;
} push;

// Hash-based false coloring — starts dark, each ID gets a distinct color
vec3 idToColor(uint id) {
    return vec3(
        float((id * 137u + 43u) % 180u + 30u) / 255.0,
        float((id * 79u  + 53u) % 180u + 20u) / 255.0,
        float((id * 241u + 97u) % 180u + 25u) / 255.0
    ) * 0.6;
}

void main() {
    if (push.objectID == 0u) discard;
    outColor = vec4(idToColor(push.objectID), 0.75);
}
