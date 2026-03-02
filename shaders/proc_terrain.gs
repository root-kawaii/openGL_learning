#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec3 fragPos;
} gs_in[];

out vec3 gFragPos;
out vec3 gNormal;

void main()
{
    // Compute flat face normal from the triangle edges
    vec3 edge1 = gs_in[1].fragPos - gs_in[0].fragPos;
    vec3 edge2 = gs_in[2].fragPos - gs_in[0].fragPos;
    vec3 flatNormal = normalize(cross(edge1, edge2));

    for (int i = 0; i < 3; ++i)
    {
        gFragPos   = gs_in[i].fragPos;
        gNormal    = flatNormal;
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
