#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// G-Buffer textures
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gDepth;
uniform samplerCube depthMap;

// Lighting uniforms
struct Light {
    vec3 Position;
    vec3 Color;
    float Linear;
    float Quadratic;
    float Radius;      // Added radius for better culling
}; 

const int NR_LIGHTS = 32;
uniform Light lights[NR_LIGHTS];
uniform int numLights;         // Actual number of lights to process
uniform vec3 viewPos;
uniform float far_plane;
uniform bool shadows;
uniform float ambientStrength; // Configurable ambient lighting
uniform float shadowBias;      // Configurable shadow bias

// Improved PCF sampling pattern - Poisson disk for better distribution
vec3 sampleOffsetDirections[20] = vec3[](
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float ShadowCalculation(vec3 fragPos, vec3 lightPos, float lightRadius)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    
    // Early exit if fragment is outside light range
    if (currentDepth > lightRadius) return 1.0;
    
    float shadow = 0.0;
    float bias = shadowBias * (1.0 + currentDepth / far_plane);
    int samples = 20;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
    
    // Adaptive sampling based on distance
    if (viewDistance > far_plane * 0.5) {
        samples = 12; // Reduce samples for distant objects
    }
    
    for(int i = 0; i < samples; ++i) {
        float closestDepth = texture(depthMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= far_plane;
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    
    return shadow / float(samples);
}

vec3 calculateLighting(vec3 fragPos, vec3 normal, vec3 albedo, float specularStrength, vec3 viewDir) {
    vec3 lighting = albedo * ambientStrength; // Configurable ambient
    
    for(int i = 0; i < min(numLights, NR_LIGHTS); ++i) {
        vec3 lightPos = lights[i].Position;
        vec3 lightColor = lights[i].Color;
        
        // Light direction and distance
        vec3 lightDir = lightPos - fragPos;
        float distance = length(lightDir);
        
        // Early light culling based on radius
        if (distance > lights[i].Radius) continue;
        
        lightDir = normalize(lightDir);
        
        // Diffuse lighting
        float NdotL = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = NdotL * albedo * lightColor;
        
        // Specular lighting (Blinn-Phong)
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float NdotH = max(dot(normal, halfwayDir), 0.0);
        float spec = pow(NdotH, 64.0); // Higher specular power for sharper highlights
        vec3 specular = lightColor * spec * specularStrength;
        
        // Attenuation
        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
        
        // Apply attenuation
        diffuse *= attenuation;
        specular *= attenuation;
        
        // Shadow calculation
        float shadow = 0.0;
        shadow = ShadowCalculation(fragPos, lightPos, lights[i].Radius);
        
        // Add contribution
        lighting += (1.0 - shadow) * (diffuse + specular);
    }
    
    return lighting;
}

void main()
{
    // Sample G-Buffer
    float depth = texture(gDepth, TexCoords).r;
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
    vec4 albedoSpec = texture(gAlbedoSpec, TexCoords);
    vec3 albedo = albedoSpec.rgb;
    float specularStrength = albedoSpec.a;
    
    // Early exit for background pixels (depth = 1.0 means sky/background)
    if (depth >= 0.999) {
        FragColor = vec4(albedo, 1.0);
        return;
    }
    
    // Reconstruct world position from depth
    vec3 fragPos = texture(gPosition, TexCoords).rgb;
    
    // Calculate view direction
    vec3 viewDir = normalize(viewPos - fragPos);
    
    // Calculate lighting
    vec3 lighting = calculateLighting(fragPos, normal, albedo, specularStrength, viewDir);
    
    // Tone mapping (simple Reinhard)
    lighting = lighting / (lighting + vec3(1.0));
    
    // Gamma correction
    lighting = pow(lighting, vec3(1.0/2.2));
    
    FragColor = vec4(lighting, 1.0);
}