#version 330 core
out vec4 FragColor;

in vec3 TexCoords; // Direction vector from the cube vertices

// This name must match skyboxShader->setInt("skybox", 5) in your C++ code
uniform samplerCube skybox; 

void main()
{    
    // We sample the cubemap using the 3D direction vector TexCoords
    FragColor = texture(skybox, TexCoords);
}