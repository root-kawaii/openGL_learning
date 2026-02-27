#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 LocalPos;
    mat3 TBN;
} fs_in;

// Lighting
struct Light {
    vec3 Position;
    vec3 Color;
};

#define MAX_LIGHTS 8
uniform Light lights[MAX_LIGHTS];
uniform int numLights;
uniform sampler2D shadowMap;
uniform vec3 viewPos;

// Terrain textures: diffuse + normal for each material
uniform sampler2D tex_grass_diff;   // unit 1  - aerial_grass_rock
uniform sampler2D tex_grass_nor;    // unit 2
uniform sampler2D tex_stone_diff;   // unit 3  - rocky_terrain
uniform sampler2D tex_stone_nor;    // unit 4
uniform sampler2D tex_rock2_diff;   // unit 5  - rocky_terrain_02 (dirt + cliff sides)
uniform sampler2D tex_rock2_nor;    // unit 6

uniform int terrainType; // 0=stone, 1=grass, 2=dirt, 3=moss
uniform float texTiling;

// Cell shading params
const int levels = 3;
const float rimPower = 3.0;
const float rimThreshold = 0.5;
const float specularThreshold = 0.8;

// ============================================================
// Noise for variation
// ============================================================

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Per-tile hash (constant within one tile)
float tileHash(vec3 worldPos) {
    vec2 tileID = floor(worldPos.xz + 0.5);
    return hash(tileID);
}

// Simple world-space UV scaling (no per-tile rotation/offset —
// at low tiling the texture spans multiple tiles naturally)
vec2 worldUV(vec2 uv) {
    return uv * texTiling;
}

// ============================================================
// Sample terrain material (diffuse + normal) by type
// ============================================================

struct TerrainSample {
    vec3 color;
    vec3 normal; // tangent-space normal from normal map
};

TerrainSample sampleMaterial(sampler2D diffTex, sampler2D norTex, vec2 uv) {
    TerrainSample s;
    s.color = texture(diffTex, uv).rgb;
    // Normal map: convert from [0,1] to [-1,1]
    s.normal = texture(norTex, uv).rgb * 2.0 - 1.0;
    return s;
}

// Sample the top face based on terrain type
TerrainSample sampleTop(vec2 uv, vec3 worldPos) {
    // uv is LocalPos.xz in [-0.5, 0.5]; remap to [0, 1] for full texture per tile
    vec2 vuv = uv + 0.5;

    TerrainSample s;
    if (terrainType == 0) {
        s = sampleMaterial(tex_stone_diff, tex_stone_nor, vuv);
    } else if (terrainType == 1) {
        s = sampleMaterial(tex_grass_diff, tex_grass_nor, vuv);
    } else if (terrainType == 2) {
        s = sampleMaterial(tex_rock2_diff, tex_rock2_nor, vuv);
    } else if (terrainType == 3) {
        s = sampleMaterial(tex_grass_diff, tex_grass_nor, vuv);
        s.color *= vec3(0.7, 0.85, 0.6);
    } else {
        s = sampleMaterial(tex_stone_diff, tex_stone_nor, vuv);
    }

    // Per-tile subtle brightness variation (+-5%)
    float th = tileHash(worldPos);
    s.color *= 0.95 + 0.1 * th;

    return s;
}

// Sample side/cliff face
TerrainSample sampleSide(vec2 uv, vec3 worldPos) {
    // World-space UVs for continuous cliff texture; 1 tile = 1 world unit
    vec2 vuv = uv * texTiling * 4.0;

    // Cliff sides always use rocky_terrain_02
    TerrainSample s = sampleMaterial(tex_rock2_diff, tex_rock2_nor, vuv);

    float th = tileHash(worldPos);
    s.color *= 0.95 + 0.1 * th;

    return s;
}

// ============================================================
// Triplanar mapping with normal map blending
// ============================================================

struct TriplanarResult {
    vec3 color;
    vec3 worldNormal; // perturbed normal in world space
};

TriplanarResult getTerrainTriplanar(vec3 worldPos, vec3 geometryNormal) {
    vec3 blendWeights = abs(geometryNormal);
    blendWeights = pow(blendWeights, vec3(6.0));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z);

    // X projection (side face) — world-space UVs so cliff texture is continuous
    TerrainSample xSample = sampleSide(worldPos.yz, worldPos);
    // Y projection (top face) — LOCAL UVs so each tile is isolated, no cross-tile blending
    TerrainSample ySample = sampleTop(fs_in.LocalPos.xz, worldPos);
    // Z projection (side face)
    TerrainSample zSample = sampleSide(worldPos.xy, worldPos);

    // Blend diffuse
    vec3 color = xSample.color * blendWeights.x
               + ySample.color * blendWeights.y
               + zSample.color * blendWeights.z;

    // Blend tangent-space normals then transform to world space via TBN
    vec3 blendedTangentNormal = xSample.normal * blendWeights.x
                              + ySample.normal * blendWeights.y
                              + zSample.normal * blendWeights.z;
    blendedTangentNormal = normalize(blendedTangentNormal);

    // Transform from tangent space to world space
    vec3 worldNormal = normalize(fs_in.TBN * blendedTangentNormal);

    // Subtle edge AO at tile borders
    vec3 edgeDist = 0.5 - abs(fs_in.LocalPos);
    float minEdge = min(edgeDist.x, edgeDist.z);
    color *= 0.95 + 0.05 * smoothstep(0.0, 0.05, minEdge);

    TriplanarResult result;
    result.color = color;
    result.worldNormal = worldNormal;
    return result;
}

// ============================================================
// Shadow calculation
// ============================================================

float ShadowCalculation(vec4 fragPosLightSpace, vec3 lightPos, vec3 normal) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if (projCoords.z > 1.0) shadow = 0.0;
    return shadow;
}

// ============================================================
// Main
// ============================================================

void main() {
    vec3 geometryNormal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);

    // Get terrain color + perturbed normal via triplanar mapping
    TriplanarResult terrain = getTerrainTriplanar(fs_in.FragPos, geometryNormal);
    vec3 terrainCol = terrain.color;
    vec3 normal = terrain.worldNormal;

    // Toon lighting using the perturbed normal
    vec3 ambient = 0.15 * terrainCol;
    vec3 totalLighting = vec3(0.0);

    for (int i = 0; i < numLights; i++) {
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);

        // Cell-shaded diffuse
        float diff = max(dot(lightDir, normal), 0.0);
        float cellDiff = floor(diff * levels) / levels;
        float diffSmooth = smoothstep(0.0, 0.1, diff - floor(diff * levels) / levels);
        cellDiff = mix(cellDiff, cellDiff + 1.0 / levels, diffSmooth * 0.3);

        // Cell-shaded specular
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
        float cellSpec = smoothstep(specularThreshold - 0.05, specularThreshold + 0.05, spec);

        // Shadow (first light only)
        float shadow = 0.0;
        if (i == 0) {
            shadow = ShadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, normal);
        }
        float cellShadow = smoothstep(0.4, 0.6, shadow);

        vec3 diffuse = cellDiff * lights[i].Color * terrainCol;
        vec3 specular = cellSpec * lights[i].Color * 0.3;

        totalLighting += mix(diffuse + specular, vec3(0.0), cellShadow);
    }

    // Rim lighting
    float rimDot = 1.0 - max(dot(viewDir, normal), 0.0);
    float rimIntensity = pow(rimDot, rimPower);
    rimIntensity = smoothstep(rimThreshold, 1.0, rimIntensity);
    vec3 rimColor = vec3(0.4, 0.7, 0.9) * rimIntensity * 0.4;

    vec3 finalColor = ambient + totalLighting + rimColor;

    // Saturation boost
    float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
    finalColor = mix(vec3(luminance), finalColor, 1.15);

    FragColor = vec4(finalColor, 1.0);
}
