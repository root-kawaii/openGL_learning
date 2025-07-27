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
uniform sampler2D gMetallic;
uniform sampler2D gRoughness;

// Camera parameters for depth reconstruction
uniform float near_plane;
uniform float far_plane;
uniform mat4 projection;
uniform mat4 view;

// uniform float metallic;
// uniform float roughness;
uniform float ao;

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

// PBR Constants
const float PI = 3.14159265359;

// Improved PCF sampling pattern - Poisson disk for better distribution
vec3 sampleOffsetDirections[20] = vec3[](
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

// PBR Functions
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

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

// Enhanced PBR lighting calculation with linear depth awareness
vec3 calculatePBRLighting(vec3 fragPos, vec3 normal, vec3 albedo, float metallic, float roughness, float ao, vec3 viewDir, float linearDepth) {
    vec3 N = normalize(normal);
    vec3 V = normalize(viewDir);
    
    // Calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
    
    // Reflectance equation
    vec3 Lo = vec3(0.0);
    
    for(int i = 0; i < min(numLights, NR_LIGHTS); ++i) {
        vec3 lightPos = lights[i].Position;
        vec3 lightColor = lights[i].Color;
        
        // Light direction and distance
        vec3 L = normalize(lightPos - fragPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - fragPos);
        
        // Early light culling based on radius
        if (distance > lights[i].Radius) continue;
        
        // Calculate per-light radiance
        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
        vec3 radiance = lightColor * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        // Shadow calculation with linear depth
        float shadow = 0.0;
        if (shadows) {
            shadow = ShadowCalculation(fragPos, lightPos, lights[i].Radius, linearDepth);
        }
        
        // Add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);        
        Lo += (1.0 - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;
    }   
    
    // Ambient lighting (we now try to fake lighting coming from the 'environment')
    // you can also use Image Based Lighting (IBL) for more realistic ambient lighting
    vec3 ambient = vec3(ambientStrength) * albedo * ao;
    
    vec3 color = ambient + Lo;
    
    return color;
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
    float metallic = texture(gMetallic, TexCoords).r;
    float roughness = texture(gRoughness, TexCoords).r;
    float specularStrength = albedoSpec.a; // This could be repurposed for metallic or roughness
    
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
    vec3 viewDir = viewPos - fragPos;
    
    // Calculate PBR lighting with linear depth awareness
    vec3 color = calculatePBRLighting(fragPos, normal, albedo, metallic, roughness, ao, viewDir, linearDepth);
    
    // Optional: Apply depth-based fog
    // vec3 fogColor = vec3(0.5, 0.6, 0.7); // Light blue fog
    // color = applyDepthFog(color, linearDepth, fogColor, far_plane * 0.7, far_plane);
    
    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}