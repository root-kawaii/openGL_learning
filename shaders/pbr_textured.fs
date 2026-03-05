#version 330 core

// ── 1. INPUTS FROM VERTEX SHADER ─────────────────────────────────────────────

in VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    mat3 TBN;
} fs_in;

out vec4 FragColor;

// ── 2. MATERIAL TEXTURES ─────────────────────────────────────────────────────

uniform sampler2D albedoMap;     
uniform sampler2D normalMap;     
uniform sampler2D metallicMap;   
uniform sampler2D roughnessMap;  
uniform sampler2D aoMap;         

// ── 3. LIGHTS AND CAMERA ─────────────────────────────────────────────────────

struct Light { vec3 Position; vec3 Color; };
#define MAX_LIGHTS 8
uniform Light       lights[MAX_LIGHTS];
uniform int         numLights;
uniform vec3        viewPos;
uniform sampler2D   shadowMap;
uniform samplerCube envMap;           
uniform sampler2D   depthMap;         
float       texTiling   = 2.0;
uniform float       heightScale = 0.1; // Increased default for more pop

// ── 4. PBR MATH FUNCTIONS ────────────────────────────────────────────────────

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

float shadowCalculation(vec4 fragPosLightSpace, vec3 lightPos, vec3 N)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    vec3  lightDir = normalize(lightPos - fs_in.WorldPos);
    float bias     = max(0.002 * (1.0 - dot(N, lightDir)), 0.0002);

    float shadow    = 0.0;
    vec2  texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float depth = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += proj.z - bias > depth ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

// ── 5. PARALLAX OCCLUSION MAPPING (FIXED & BOOSTED) ──────────────────────────

vec2 parallaxOcclusionMapping(vec2 texCoords, vec3 viewDir)
{
    // High layer counts to prevent artifacts at high heightScale
    const float minLayers = 16.0;
    const float maxLayers = 64.0;
    float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));

    float layerDepth      = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    vec2 P             = viewDir.xy * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2  currentTexCoords     = texCoords;
    
    // We use pow() to make the "valleys" deeper and stones "rounder"
    float rawH = 1.0 - texture(depthMap, currentTexCoords).r;
    float currentDepthMapValue = pow(rawH, 1.8); 

    while (currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords     -= deltaTexCoords;
        rawH                  = 1.0 - texture(depthMap, currentTexCoords).r;
        currentDepthMapValue  = pow(rawH, 1.8); 
        currentLayerDepth    += layerDepth;
    }

    // Correcting interpolation to match the power-mapped height
    vec2  prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth    = currentDepthMapValue - currentLayerDepth;
    float prevRawH      = 1.0 - texture(depthMap, prevTexCoords).r;
    float beforeDepth   = pow(prevRawH, 1.8) - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentTexCoords, prevTexCoords, weight);
}

// ── 6. MAIN ──────────────────────────────────────────────────────────────────

void main()
{
    // ── 6a. Parallax + UV ──
    vec3 viewDirTangent = normalize(transpose(fs_in.TBN) * (viewPos - fs_in.WorldPos));
    vec2 baseUV = fs_in.TexCoords * texTiling;
    vec2 uv     = (heightScale > 0.0) ? parallaxOcclusionMapping(baseUV, viewDirTangent) : baseUV;
    uv = fract(uv);

    // ── 6b. Material Sampling ──
    vec3  albedo    = pow(texture(albedoMap, uv).rgb, vec3(2.2));
    vec3  tangentNormal = texture(normalMap, uv).rgb * 2.0 - 1.0;
    vec3  N = (tangentNormal.z > 0.1) ? normalize(fs_in.TBN * tangentNormal) : normalize(fs_in.Normal);

    float metallic  = texture(metallicMap,  uv).b;
    float roughness = texture(roughnessMap, uv).g;
    
    // EXTREME AO: Remapping AO for the "Inked" look
    float rawAO = texture(aoMap, uv).r;
    float ao    = pow(rawAO, 5.0); // Extreme contrast for indirect light
    float directAOMask = pow(rawAO, 5.0); // Hard shadows for direct light

    vec3 V = normalize(viewPos - fs_in.WorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── 6c. Lighting ──
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < numLights; i++)
    {
        vec3  L = normalize(lights[i].Position - fs_in.WorldPos);
        vec3  H = normalize(V + L);

        float dist        = length(lights[i].Position - fs_in.WorldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3  radiance    = lights[i].Color * attenuation;

        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  specular = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float shadow = (i == 0) ? shadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, N) : 0.0;
        float NdotL = max(dot(N, L), 0.0);

        // AO applied here to direct light (Lo) prevents wash-out in crevices
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow) * directAOMask;
    }

    // ── 6d. Environment ──
    vec3  R        = reflect(-V, N);
    vec3  envColor = pow(textureLod(envMap, R, roughness * 7.0).rgb, vec3(2.2));
    vec3  kS_env   = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3  envSpecular = kS_env * envColor * (1.0 - roughness);

    // Higher ambient floor to keep stylized colors rich
    vec3 ambient = vec3(0.04) * albedo * ao;

    vec3 color = ambient + Lo + (envSpecular * ao);

    // ── 6e. Final Tone Mapping + Gamma ──
    color = color / (color + vec3(1.0));   
    color = pow(color, vec3(1.0 / 2.2));   

    FragColor = vec4(color, 1.0);
}