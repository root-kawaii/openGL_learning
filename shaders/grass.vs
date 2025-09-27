#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

// Instance attributes
layout (location = 3) in vec3 instancePosition;
layout (location = 4) in float instanceRotation;
layout (location = 5) in float instanceScale;
layout (location = 6) in vec3 instanceTint;

uniform mat3 model;
uniform mat4 projection;
uniform mat4 view;
uniform float time;
uniform float grassHeight;
uniform float windSpeed;
uniform float windStrength;
uniform vec2 windFrequency;
uniform vec2 windDirection;
uniform float windTurbulence;
uniform float windGustiness;
uniform sampler2D windDistortionMap;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec3 GrassTint;
out float GrassAlpha;
out vec3 WorldPos;

void main() {
    // Create rotation matrix for instance
    float c = cos(instanceRotation);
    float s = sin(instanceRotation);
    mat3 rotationMatrix = mat3(
        c, 0, s,
        0, 1, 0,
        -s, 0, c
    );
    
    // Apply instance transformations
    vec3 rotatedPos = rotationMatrix * model * (aPos * instanceScale);
    vec3 worldPos = rotatedPos + instancePosition;
    
    // Wind animation
    float windSample = texture(windDistortionMap, 
        (instancePosition.xz * windFrequency) + (time * windSpeed * windDirection)).r;
    
    // Complex wind calculation
    float windEffect = windStrength * windSample * windGustiness;
    float heightFactor = (aPos.y + 0.5) / grassHeight; // Assuming grass goes from -0.5 to grassHeight-0.5
    heightFactor = pow(heightFactor, 2.0); // Quadratic falloff
    
    // Multi-layer wind distortion
    vec2 windOffset1 = windDirection * windEffect * heightFactor;
    vec2 windOffset2 = vec2(-windDirection.y, windDirection.x) * windEffect * windTurbulence * heightFactor;
    
    worldPos.x += windOffset1.x + windOffset2.x * sin(time * 2.0 + instancePosition.x * 0.1);
    worldPos.z += windOffset1.y + windOffset2.y * cos(time * 1.5 + instancePosition.z * 0.1);
    
    FragPos = worldPos;
    WorldPos = worldPos;
    Normal = rotationMatrix * aNormal;
    TexCoord = aTexCoord;
    GrassTint = instanceTint;
    
    // Calculate alpha based on distance and height
    GrassAlpha = 1.0 - smoothstep(0.8, 1.0, heightFactor * windEffect);
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
}