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






// #version 330 core
// out vec4 FragColor;

// in vec2 TexCoords;

// // G-Buffer textures
// uniform sampler2D gPosition;
// uniform sampler2D gNormal;
// uniform sampler2D gAlbedoSpec;
// uniform sampler2D gLinearDepth;
// uniform samplerCube depthMap;
// uniform sampler2D gDepth;
// uniform sampler2D gMetallic;
// uniform sampler2D gRoughness;

// // Camera parameters for depth reconstruction
// uniform float near_plane;
// uniform float far_plane;
// uniform mat4 projection;
// uniform mat4 view;

// uniform float ao;

// // Lighting uniforms
// struct Light {
//     vec3 Position;
//     vec3 Color;
//     float Linear;
//     float Quadratic;
//     float Radius;
// }; 

// const int NR_LIGHTS = 32;
// uniform Light lights[NR_LIGHTS];
// uniform int numLights;
// uniform vec3 viewPos;
// uniform bool shadows;
// uniform float ambientStrength;
// uniform float shadowBias;

// // Cell shading parameters
// uniform float toonLevels = 10.0;        // Number of toon shading levels
// uniform float toonThreshold = 0.91;     // Threshold for edge detection
// uniform vec3 outlineColor = vec3(0.0, 0.0, 0.0);  // Outline color
// uniform float outlineThickness = 1.0;  // Outline thickness multiplier
// uniform bool enableOutlines = true;    // Enable/disable outlines
// uniform float specularThreshold = 0.8; // Threshold for specular highlights
// uniform float specularSmoothness = 0.01; // Smoothness of specular transition

// // PBR Constants
// const float PI = 3.14159265359;

// // Improved PCF sampling pattern
// vec3 sampleOffsetDirections[20] = vec3[](
//    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
//    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
//    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
//    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
//    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
// );

// // Quantize a value to specific levels for toon shading
// float quantize(float value, float levels) {
//     return floor(value * levels) / levels;
// }

// // Smooth step function for toon transitions
// float toonStep(float edge, float value, float smoothness) {
//     return smoothstep(edge - smoothness, edge + smoothness, value);
// }

// // Sample neighboring pixels for edge detection
// vec2 getTexelSize() {
//     return 1.0 / textureSize(gDepth, 0);
// }

// // Sobel edge detection for outlines
// float getEdgeIntensity(vec2 texCoords) {
//     vec2 texelSize = getTexelSize() * outlineThickness;
    
//     // Sample depth values in a 3x3 grid
//     float tl = texture(gLinearDepth, texCoords + vec2(-texelSize.x, -texelSize.y)).r; // top-left
//     float tm = texture(gLinearDepth, texCoords + vec2(0.0, -texelSize.y)).r;         // top-middle
//     float tr = texture(gLinearDepth, texCoords + vec2(texelSize.x, -texelSize.y)).r;  // top-right
//     float ml = texture(gLinearDepth, texCoords + vec2(-texelSize.x, 0.0)).r;         // middle-left
//     float mm = texture(gLinearDepth, texCoords).r;                                   // middle-middle
//     float mr = texture(gLinearDepth, texCoords + vec2(texelSize.x, 0.0)).r;          // middle-right
//     float bl = texture(gLinearDepth, texCoords + vec2(-texelSize.x, texelSize.y)).r;  // bottom-left
//     float bm = texture(gLinearDepth, texCoords + vec2(0.0, texelSize.y)).r;          // bottom-middle
//     float br = texture(gLinearDepth, texCoords + vec2(texelSize.x, texelSize.y)).r;   // bottom-right
    
//     // Sobel X kernel
//     float sobelX = -1.0*tl + 0.0*tm + 1.0*tr +
//                    -2.0*ml + 0.0*mm + 2.0*mr +
//                    -1.0*bl + 0.0*bm + 1.0*br;
    
//     // Sobel Y kernel
//     float sobelY = -1.0*tl + -2.0*tm + -1.0*tr +
//                     0.0*ml +  0.0*mm +  0.0*mr +
//                     1.0*bl +  2.0*bm +  1.0*br;
    
//     float edge = sqrt(sobelX * sobelX + sobelY * sobelY);
    
//     // Also check normal discontinuities
//     vec3 normalCenter = texture(gNormal, texCoords).rgb;
//     vec3 normalRight = texture(gNormal, texCoords + vec2(texelSize.x, 0.0)).rgb;
//     vec3 normalUp = texture(gNormal, texCoords + vec2(0.0, texelSize.y)).rgb;
    
//     float normalEdge = 1.0 - dot(normalCenter, normalRight) + 1.0 - dot(normalCenter, normalUp);
    
//     return max(edge, normalEdge * 0.5);
// }

// float ShadowCalculation(vec3 fragPos, vec3 lightPos, float lightRadius, float linearDepth)
// {
//     vec3 fragToLight = fragPos - lightPos;
//     float currentDepth = length(fragToLight);
    
//     if (currentDepth > lightRadius) return 1.0;
    
//     float shadow = 0.0;
//     float bias = shadowBias * (1.0 + linearDepth / far_plane);
//     int samples = 20;
//     float diskRadius = (1.0 + (linearDepth / far_plane)) / 25.0;
    
//     if (linearDepth > far_plane * 0.5) {
//         samples = 12;
//     }
    
//     for(int i = 0; i < samples; ++i) {
//         float closestDepth = texture(depthMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
//         closestDepth *= far_plane;
//         if(currentDepth - bias > closestDepth)
//             shadow += 1.0;
//     }
    
//     // Quantize shadow for toon effect
//     shadow = shadow / float(samples);
//     return quantize(shadow, 2.0); // Binary shadow for toon look
// }

// // Simplified toon lighting calculation
// vec3 calculateToonLighting(vec3 fragPos, vec3 normal, vec3 albedo, float metallic, float roughness, float ao, vec3 viewDir, float linearDepth) {
//     vec3 N = normalize(normal);
//     vec3 V = normalize(viewDir);
    
//     vec3 totalLight = vec3(0.0);
    
//     for(int i = 0; i < min(numLights, NR_LIGHTS); ++i) {
//         vec3 lightPos = lights[i].Position;
//         vec3 lightColor = lights[i].Color;
        
//         vec3 L = normalize(lightPos - fragPos);
//         vec3 H = normalize(V + L);
//         float distance = length(lightPos - fragPos);
        
//         if (distance > lights[i].Radius) continue;
        
//         // Calculate basic lighting terms
//         float NdotL = max(dot(N, L), 0.0);
//         float NdotH = max(dot(N, H), 0.0);
        
//         // Quantize the diffuse term for toon look
//         float toonDiffuse = quantize(NdotL, toonLevels);
        
//         // Create sharp specular highlights
//         float specular = pow(NdotH, 32.0 * (1.0 - roughness));
//         float toonSpecular = toonStep(specularThreshold, specular, specularSmoothness);
        
//         // Calculate attenuation
//         float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
//         attenuation = quantize(attenuation, toonLevels); // Quantize attenuation too
        
//         // Shadow calculation
//         float shadow = 0.0;
//         if (shadows) {
//             shadow = ShadowCalculation(fragPos, lightPos, lights[i].Radius, linearDepth);
//         }
        
//         // Combine lighting components
//         vec3 diffuse = toonDiffuse * albedo;
//         vec3 spec = toonSpecular * mix(vec3(0.04), albedo, metallic);
        
//         totalLight += (1.0 - shadow) * (diffuse + spec) * lightColor * attenuation;
//     }
    
//     // Quantized ambient lighting
//     vec3 ambient = vec3(quantize(ambientStrength, toonLevels)) * albedo * ao;
    
//     return ambient + totalLight;
// }

// void main()
// {
//     // Sample G-Buffer
//     float depth = texture(gDepth, TexCoords).r;
//     float linearDepth = texture(gLinearDepth, TexCoords).r;
//     vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
//     vec4 albedoSpec = texture(gAlbedoSpec, TexCoords);
//     vec3 albedo = albedoSpec.rgb;
//     float metallic = texture(gMetallic, TexCoords).r;
//     float roughness = texture(gRoughness, TexCoords).r;
    
//     // Early exit for background pixels
//     if (depth >= 0.999) {
//         FragColor = vec4(albedo, 1.0);
//         return;
//     }
    
//     // Get world position
//     vec3 fragPos = texture(gPosition, TexCoords).rgb;
//     vec3 viewDir = viewPos - fragPos;
    
//     // Calculate toon lighting
//     vec3 color = calculateToonLighting(fragPos, normal, albedo, metallic, roughness, ao, viewDir, linearDepth);
    
//     // Edge detection for outlines
//     if (enableOutlines) {
//         float edgeIntensity = getEdgeIntensity(TexCoords);
        
//         if (edgeIntensity > toonThreshold) {
//             color = mix(color, outlineColor, edgeIntensity);
//         }
//     }
    
//     // Optional: Apply depth-based fog with quantization
//     // vec3 fogColor = vec3(0.7, 0.8, 0.9);
//     // float fogFactor = clamp((far_plane - linearDepth) / (far_plane * 0.3), 0.0, 1.0);
//     // fogFactor = quantize(fogFactor, toonLevels);
//     // color = mix(fogColor, color, fogFactor);
    
//     // Skip HDR tonemapping for more vibrant toon colors
//     // Instead, clamp values to maintain the toon aesthetic
//     color = clamp(color, 0.0, 1.0);
    
//     // Reduce gamma correction for more vibrant colors
//     color = pow(color, vec3(1.0/1.8));
    
//     FragColor = vec4(color, 1.0);
// }