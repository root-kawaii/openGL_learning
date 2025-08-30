#version 330 core

in vec2 TexCoord;
in vec3 WorldPos;
in vec4 ClipSpacePos;

uniform sampler2D depthTexture;
uniform sampler2D foamTexture;
uniform vec3 cameraPos;
uniform float time;
uniform float nearPlane;
uniform float farPlane;

// Water properties
uniform vec3 waterColorShallow;
uniform vec3 waterColorDeep;
uniform float waterTransparency;
uniform float foamThreshold;
uniform float foamStrength;

out vec4 FragColor;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float CalculateFoam(float depthDiff, vec2 worldXZ)
{
    // Create a sharper foam boundary
    float foamDistance = 1.5; // Distance from object where foam appears
    float foamFalloff = 0.3;   // How quickly foam fades
    
    // Sharp foam mask with smooth falloff
    float foamMask = 1.0 - smoothstep(0.0, foamDistance, depthDiff);
    foamMask = pow(foamMask, 0.5); // Make the falloff more pronounced
    
    // Sample or generate noise for foam texture
    vec2 foamUV1 = worldXZ * 0.1 + time * 0.05;
    vec2 foamUV2 = worldXZ * 0.15 + time * -0.03;
    
    float foamNoise;
    
    // Try to use foam texture, fallback to procedural
    float foam1 = texture(foamTexture, foamUV1).r;
    float foam2 = texture(foamTexture, foamUV2).r;
    
    // If texture sampling returns near 0 (no texture), use procedural
    if (foam1 < 0.1 && foam2 < 0.1) {
        // Generate better procedural foam noise
        float noise1 = fract(sin(dot(foamUV1, vec2(12.9898, 78.233))) * 43758.5453);
        float noise2 = fract(sin(dot(foamUV2, vec2(93.9898, 67.233))) * 28758.5453);
        foamNoise = noise1 * noise2;
        foamNoise = smoothstep(0.3, 0.8, foamNoise); // Make noise more contrasted
    } else {
        foamNoise = foam1 * foam2;
    }
    
    // Combine mask and noise
    float finalFoam = foamMask * foamNoise * foamStrength;
    
    // Add a clean edge component for sharp outline
    float sharpEdge = 1.0 - smoothstep(0.0, 0.2, depthDiff);
    sharpEdge = pow(sharpEdge, 2.0);
    
    return max(finalFoam, sharpEdge * 0.8);
}

void main()
{
    // Calculate screen space UV for depth sampling
    vec2 screenUV = (ClipSpacePos.xy / ClipSpacePos.w) * 0.5 + 0.5;
    
    // Sample scene depth and calculate water depth
    float sceneDepthRaw = texture(depthTexture, screenUV).r;
    float waterDepthRaw = gl_FragCoord.z;
    
    // Linearize both depths
    float sceneDepth = LinearizeDepth(sceneDepthRaw);
    float waterDepth = LinearizeDepth(waterDepthRaw);
    
    // Calculate depth difference
    float depthDiff = sceneDepth - waterDepth;
    
    // DEBUG: Uncomment to visualize depth difference
    // FragColor = vec4(depthDiff * 0.5, depthDiff * 0.5, depthDiff * 0.5, 1.0);
    // return;
    
    // Calculate water depth factor for color variation
    float waterDepthFactor = clamp(depthDiff * 0.1, 0.0, 1.0);
    
    // Simple fresnel effect
    vec3 viewDir = normalize(cameraPos - WorldPos);
    vec3 surfaceNormal = vec3(0.0, 1.0, 0.0);
    float fresnel = pow(1.0 - max(dot(viewDir, surfaceNormal), 0.0), 1.5);
    
    // Calculate base water color
    vec3 waterColor = mix(waterColorShallow, waterColorDeep, waterDepthFactor);
    
    // Calculate foam with sharp outline
    float foam = CalculateFoam(depthDiff, WorldPos.xz);
    
    // Final color blending - make foam more prominent
    vec3 foamColor = vec3(0.95, 0.98, 1.0); // Slightly blue-tinted white
    vec3 finalColor = mix(waterColor, foamColor, foam);
    
    // Calculate final alpha
    float alpha = waterTransparency + fresnel * 0.15 + foam * 0.2;
    alpha = clamp(alpha, 0.4, 0.9);
    
    FragColor = vec4(finalColor, alpha);
}