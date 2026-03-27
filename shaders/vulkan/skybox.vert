#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoords;

layout(push_constant) uniform PushConstants {
    mat4 viewProj; // projection * mat4(mat3(view)) — no translation
} push;

void main() {
    fragTexCoords = inPosition;
    vec4 pos = push.viewProj * vec4(inPosition, 1.0);
    // Set z = w so depth is always 1.0 (far plane) after perspective divide
    gl_Position = pos.xyww;
}
