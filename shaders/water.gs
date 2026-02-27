#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

// Inputs from water.vs (arrays because geometry shader receives a whole primitive)
in vec3 FragPos[];
in vec3 Normal[];
in vec2 TexCoords[];
in vec4 ClipSpacePos[];

// Outputs to water_forward.fs
out vec3 gFragPos;
out vec3 gNormal;
out vec2 gTexCoords;
out vec4 gClipSpacePos;

void main()
{
    // Compute the true face normal from the three deformed world-space positions.
    // This is more accurate than the vertex shader's numerical approximation
    // because we have access to all three vertices of the triangle after displacement.
    vec3 edge0 = FragPos[1] - FragPos[0];
    vec3 edge1 = FragPos[2] - FragPos[0];
    vec3 faceNormal = normalize(cross(edge0, edge1));

    // Always point upward — water surface faces +Y
    if (faceNormal.y < 0.0)
        faceNormal = -faceNormal;

    // Re-emit each vertex with the corrected face normal
    for (int i = 0; i < 3; i++)
    {
        gl_Position   = gl_in[i].gl_Position;
        gFragPos      = FragPos[i];
        gNormal       = faceNormal;     // flat per-face normal; fragment shader adds micro-detail
        gTexCoords    = TexCoords[i];
        gClipSpacePos = ClipSpacePos[i];
        EmitVertex();
    }
    EndPrimitive();
}
