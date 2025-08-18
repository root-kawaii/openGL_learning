#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 model; // Used for translation, rotation, and scale
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // The model matrix now contains both translation, rotation, and scale.
    // The billboarding effect will override the camera's rotation.
    
    // Copy the view matrix but remove the rotation part.
    mat4 billboardView = view;
    billboardView[0][0] = 1.0; billboardView[0][1] = 0.0; billboardView[0][2] = 0.0;
    billboardView[1][0] = 0.0; billboardView[1][1] = 1.0; billboardView[1][2] = 0.0;
    billboardView[2][0] = 0.0; billboardView[2][1] = 0.0; billboardView[2][2] = 1.0;

    // Apply model transformation (translation, rotation, and scale)
    // and then the billboarded view and projection matrices.
    gl_Position = projection * billboardView * model * vec4(aPos, 1.0);
    
    TexCoords = aTexCoords;
}