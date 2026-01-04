#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} fs_in;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 sandColor;     // Set this to vec3(0.76, 0.7, 0.5) for sand
uniform vec3 fogColor;      // Match your sky/background color
uniform float fogDensity;   // Set to a very small number (e.g., 0.0001) for large planes

void main() {
    // 1. Basic Lighting
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // 2. Ambient (Prevents the "pure black" shadows)
    vec3 ambient = 0.3 * sandColor;
    vec3 diffuse = diff * sandColor;
    
    vec3 result = ambient + diffuse;

    // 3. Fog Calculation (Crucial for the Blue issue)
    // Distance from camera to fragment
    float dist = length(viewPos - fs_in.FragPos);
    // Linear fog: increase fogStart and fogEnd if you scale your plane up
    float fogStart = 100.0;
    float fogEnd = 5000.0; 
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);

    // Mix the sand color with the fog/sky color
    result = mix(result, fogColor, fogFactor);

    FragColor = vec4(result, 1.0);
}