#version 450

// Outputs UV to fragment shader
layout(location = 0) out vec2 fragUV;

// Generates fullscreen triangle that covers the screen
// 3 vertices cover entire NDC space without needing a vertex buffer
void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    fragUV      = uvs[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
