#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform vec4 clipPlane;

void main()
{
    vec4 worldPos   = model * vec4(aPos, 1.0);
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
    FragPos         = worldPos.xyz;
    Normal          = vec3(0.0, 1.0, 0.0); // flat plane — normal always up
    TexCoords       = aTexCoords;
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position     = projection * view * worldPos;
}
