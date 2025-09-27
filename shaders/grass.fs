#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec3 GrassTint;
in float GrassAlpha;
in vec3 WorldPos;

uniform sampler2D grassTexture;
uniform sampler2D grassMask;
uniform sampler2D groundTexture;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 cameraPos;
uniform float translucentGain;
uniform float translucentPower;
uniform vec3 subsurfaceColor;
uniform float alphaThreshold;
uniform float grassMaskThreshold;
uniform float seasonalTint;
uniform float healthVariation;
uniform float dryness;

out vec4 FragColor;

void main() {
    // Sample textures
    vec4 grassColor = texture(grassTexture, TexCoord);
    float maskValue = texture(grassMask, TexCoord).r;
    
    // Early discard based on mask
    if (maskValue < grassMaskThreshold || grassColor.a < alphaThreshold) {
        discard;
    }
    
    // Apply instance tinting
    grassColor.rgb *= GrassTint;
    
    // Seasonal and environmental effects
    vec3 seasonalColor = mix(grassColor.rgb, vec3(0.8, 0.6, 0.2), seasonalTint);
    vec3 drynessColor = mix(seasonalColor, vec3(0.6, 0.5, 0.3), dryness);
    
    // Lighting calculations
    vec3 norm = normalize(Normal);
    vec3 lightDirection = normalize(-lightDir);
    
    // Diffuse lighting
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Subsurface scattering approximation
    vec3 viewDir = normalize(cameraPos - FragPos);
    float subsurface = pow(max(dot(-lightDirection + norm, viewDir), 0.0), translucentPower);
    vec3 subsurfaceLight = subsurface * translucentGain * subsurfaceColor;
    
    // Combine lighting
    vec3 lighting = ambientColor + diffuse + subsurfaceLight;
    vec3 finalColor = drynessColor * lighting;
    
    // Apply health variation
    finalColor = mix(finalColor, finalColor * 0.7, healthVariation);
    
    FragColor = vec4(finalColor, grassColor.a * GrassAlpha);
}