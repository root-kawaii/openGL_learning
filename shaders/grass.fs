#version 330 core
in vec2 FragTexCoord;
in vec3 FragWorldPos;
in float GrassHeight;
in vec3 FragNormal;

uniform sampler2D groundTexture;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float translucentGain;
uniform float alphaThreshold;

out vec4 FragColor;

void main() {
    // Sample ground texture
    vec4 texColor = texture(groundTexture, FragTexCoord);
    
    // Create procedural alpha for grass blade shape
    float edgeDistance = abs(FragTexCoord.x - 0.5) * 2.0;
    float edgeFalloff = 1.0 - smoothstep(0.8, 1.0, edgeDistance);
    float heightTaper = 1.0 - pow(GrassHeight, 1.5);
    heightTaper = clamp(heightTaper, 0.1, 1.0);
    
    float proceduralAlpha = edgeFalloff * heightTaper;
    float finalAlpha = texColor.a * proceduralAlpha;
    
    if (finalAlpha < alphaThreshold) {
        discard;
    }
    
    // Determine face direction (Unity-style two-sided lighting)
    vec3 normal = normalize(FragNormal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    
    // Unity-style lighting calculation
    vec3 lightDirection = normalize(-lightDir);
    
    // Diffuse with translucency (subsurface scattering approximation)
    float NdotL = max(dot(normal, lightDirection), 0.0);
    float translucency = max(dot(normal, -lightDirection), 0.0) * translucentGain;
    float lighting = clamp(NdotL + translucency, 0.0, 1.0);
    
    // Apply lighting
    vec3 diffuse = lighting * lightColor;
    
    // Simple ambient (in Unity this would be SH lighting)
    vec3 ambient = vec3(0.2, 0.25, 0.3); // Subtle blue-tinted ambient
    
    // Combine lighting
    vec3 finalLighting = diffuse + ambient + 0.01; // Small constant to prevent pure black
    
    // Apply to texture
    vec3 finalColor = texColor.rgb * finalLighting;
    
    // Simple ambient occlusion at base
    float ao = mix(0.7, 1.0, pow(GrassHeight, 0.5));
    finalColor *= ao;
    
    FragColor = vec4(finalColor, finalAlpha);
}