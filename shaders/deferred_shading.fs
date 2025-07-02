#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// G-Buffer textures
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gLinearDepth;  // Added linear depth texture
uniform samplerCube depthMap;
uniform sampler2D gDepth;



// Camera parameters for depth reconstruction
uniform float near_plane;
uniform float far_plane;
uniform mat4 projection;
uniform mat4 view;

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

// Convert non-linear depth to linear depth
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

// Reconstruct world position from linear depth
vec3 ReconstructWorldPos(vec2 texCoords, float linearDepth)
{
    // Convert to NDC
    vec2 ndc = texCoords * 2.0 - 1.0;
    
    // Create view space position
    vec4 viewPos = inverse(projection) * vec4(ndc, -1.0, 1.0);
    viewPos /= viewPos.w;
    
    // Scale by linear depth
    viewPos.z = -linearDepth;
    
    // Transform to world space
    vec4 worldPos = inverse(view) * viewPos;
    return worldPos.xyz;
}

float ShadowCalculation(vec3 fragPos, vec3 lightPos, float lightRadius, float linearDepth)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    
    // Early exit if fragment is outside light range
    if (currentDepth > lightRadius) return 1.0;
    
    float shadow = 0.0;
    // Use linear depth for better bias calculation
    float bias = shadowBias * (1.0 + linearDepth / far_plane);
    int samples = 20;
    
    // Adaptive disk radius based on linear depth
    float diskRadius = (1.0 + (linearDepth / far_plane)) / 25.0;
    
    // Adaptive sampling based on linear depth
    if (linearDepth > far_plane * 0.5) {
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

// Enhanced lighting calculation with linear depth awareness
vec3 calculateLighting(vec3 fragPos, vec3 normal, vec3 albedo, float specularStrength, vec3 viewDir, float linearDepth) {
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
        
        // Specular lighting (Blinn-Phong) with depth-based falloff
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float NdotH = max(dot(normal, halfwayDir), 0.0);
        // Adjust specular power based on distance for more realistic falloff
        float specPower = mix(64.0, 32.0, linearDepth / far_plane);
        float spec = pow(NdotH, specPower);
        vec3 specular = lightColor * spec * specularStrength;
        
        // Attenuation
        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
        
        // Apply attenuation
        diffuse *= attenuation;
        specular *= attenuation;
        
        // Shadow calculation with linear depth
        float shadow = 0.0;
        if (shadows) {
            shadow = ShadowCalculation(fragPos, lightPos, lights[i].Radius, linearDepth);
        }
        
        // Add contribution
        lighting += (1.0 - shadow) * (diffuse + specular);
    }
    
    return lighting;
}

// Depth-based fog calculation
vec3 applyDepthFog(vec3 color, float linearDepth, vec3 fogColor, float fogStart, float fogEnd) {
    float fogFactor = clamp((fogEnd - linearDepth) / (fogEnd - fogStart), 0.0, 1.0);
    return mix(fogColor, color, fogFactor);
}

void main()
{
    // Sample G-Buffer
    float depth = texture(gDepth, TexCoords).r;
    float linearDepth = texture(gLinearDepth, TexCoords).r; // Sample linear depth
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
    vec4 albedoSpec = texture(gAlbedoSpec, TexCoords);
    vec3 albedo = albedoSpec.rgb;
    float specularStrength = albedoSpec.a;
    
    // Early exit for background pixels (depth = 1.0 means sky/background)
    if (depth >= 0.999) {
        FragColor = vec4(albedo, 1.0);
        return;
    }
    
    // Reconstruct world position - use stored position or reconstruct from linear depth
    vec3 fragPos = texture(gPosition, TexCoords).rgb;
    
    // Alternative: reconstruct from linear depth if position isn't stored
    // vec3 fragPos = ReconstructWorldPos(TexCoords, linearDepth);
    
    // Calculate view direction
    vec3 viewDir = normalize(viewPos - fragPos);
    
    // Calculate lighting with linear depth awareness
    vec3 lighting = calculateLighting(fragPos, normal, albedo, specularStrength, viewDir, linearDepth);
    
    // Optional: Apply depth-based fog
    // vec3 fogColor = vec3(0.5, 0.6, 0.7); // Light blue fog
    // lighting = applyDepthFog(lighting, linearDepth, fogColor, far_plane * 0.7, far_plane);
    
    // Tone mapping (simple Reinhard)
    lighting = lighting / (lighting + vec3(1.0));
    
    // Gamma correction
    lighting = pow(lighting, vec3(1.0/2.2));
    
    FragColor = vec4(lighting, 1.0);
}