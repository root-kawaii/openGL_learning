#version 450

// Output: object ID as an unsigned integer
layout(location = 0) out uint outObjectID;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint objectID;
} push;

void main() {
    outObjectID = push.objectID;
}
