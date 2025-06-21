#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform mat4 viewProjection;
uniform vec3 cameraWorldPos;
uniform vec2 screenSize;
uniform float time;

const vec3 waterColor = vec3(0.1, 0.3, 0.6);
const float waterAlpha = 0.7;

// Simple screen-space reflection
vec3 GetReflection(vec3 worldPos, vec3 normal) {
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    
    // Calculate view direction and reflection
    vec3 viewDir = normalize(cameraWorldPos - worldPos);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Project reflection direction to screen space
    vec4 reflectWorldPos = vec4(worldPos + reflectDir * 2.0, 1.0);
    vec4 reflectScreenPos = viewProjection * reflectWorldPos;
    
    if (reflectScreenPos.w > 0.0) {
        reflectScreenPos /= reflectScreenPos.w;
        vec2 reflectUV = reflectScreenPos.xy * 0.5 + 0.5;
        
        // Check if the reflection UV is within screen bounds
        if (reflectUV.x >= 0.0 && reflectUV.x <= 1.0 && 
            reflectUV.y >= 0.0 && reflectUV.y <= 1.0) {
            return texture(gAlbedoSpec, reflectUV).rgb;
        }
    }
    
    // Fallback: sample scene color with slight offset for fake reflection
    vec2 offsetUV = screenUV + normal.xz * 0.05;
    offsetUV = clamp(offsetUV, 0.0, 1.0);
    return texture(gAlbedoSpec, offsetUV).rgb * 0.8;
}

void main() {
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    
    // Get animated water normal
    vec3 waterNormal = normalize(Normal);
    
    // Add wave animation
    vec2 waveOffset = sin(FragPos.xz * 3.0 + time * 2.0) * 0.02;
    waterNormal.x += waveOffset.x;
    waterNormal.z += waveOffset.y;
    waterNormal = normalize(waterNormal);
    
    // Get reflection color
    vec3 reflectionColor = GetReflection(FragPos, waterNormal);
    
    // Get refraction (scene behind water with slight distortion)
    vec2 distortedUV = screenUV + waterNormal.xz * 0.03;
    distortedUV = clamp(distortedUV, 0.0, 1.0);
    vec3 refractionColor = texture(gAlbedoSpec, distortedUV).rgb;
    
    // Calculate fresnel
    vec3 viewDir = normalize(cameraWorldPos - FragPos);
    float fresnel = pow(1.0 - max(dot(viewDir, waterNormal), 0.0), 3.0);
    
    // Mix colors
    vec3 finalColor = mix(refractionColor, reflectionColor, fresnel * 0.6);
    finalColor = mix(finalColor, waterColor, 0.4);
    
    // Add some sparkle/shimmer
    float sparkle = sin(FragPos.x * 10.0 + time) * sin(FragPos.z * 10.0 + time) * 0.1 + 0.9;
    finalColor *= sparkle;
    
    FragColor = vec4(finalColor, waterAlpha);
}