#version 330 core
layout (location = 0) in vec3 aPos;        // 3D position
layout (location = 1) in vec3 aNormal;     // Normal vector
layout (location = 2) in vec2 aTexCoords;  // Texture coordinates

// Output to fragment shader
out vec2 TexCoords;
out vec3 FragPos;    // World space position
out vec3 Normal;     // Normal vector

// Transformation matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Pass texture coordinates to fragment shader
    TexCoords = aTexCoords;
    
    // Calculate world space position
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Transform normal to world space
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // Transform vertex position to clip space
    gl_Position = projection * view * vec4(FragPos, 1.0);
}