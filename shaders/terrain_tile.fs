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

struct Light { vec3 Position; vec3 Color; };
#define MAX_LIGHTS 8
uniform Light lights[MAX_LIGHTS];
uniform int   numLights;
uniform sampler2D shadowMap;
uniform vec3  viewPos;

// ── Diffuse + Normal (always present) ──────────────────────────────────────
uniform sampler2D tex_grass_diff;   // unit 1
uniform sampler2D tex_grass_nor;    // unit 2
uniform sampler2D tex_stone_diff;   // unit 3
uniform sampler2D tex_stone_nor;    // unit 4
uniform sampler2D tex_rock2_diff;   // unit 5
uniform sampler2D tex_rock2_nor;    // unit 6

// ── AO + Roughness (optional – enabled by has* flags) ──────────────────────
uniform sampler2D tex_grass_ao;     // unit 7
uniform sampler2D tex_stone_ao;     // unit 8
uniform sampler2D tex_grass_rough;  // unit 9
uniform sampler2D tex_stone_rough;  // unit 10

uniform bool hasGrassAO;
uniform bool hasStoneAO;
uniform bool hasGrassRough;
uniform bool hasStoneRough;

flat in int iTerrainType;
uniform float texTiling;

const int   LEVELS            = 3;
const float RIM_POWER         = 3.0;
const float RIM_THRESHOLD     = 0.5;
const float SPEC_THRESHOLD    = 0.80;

// ── Hash / Noise ─────────────────────────────────────────────────────────────

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}
vec2 hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}
float noise(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i+vec2(1,0)), f.x),
               mix(hash(i+vec2(0,1)), hash(i+vec2(1,1)), f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int oct = 0; oct < 4; oct++) {
        v += a * noise(p); p = p * 2.3 + vec2(1.7, 9.2); a *= 0.52;
    }
    return v;
}

// ── Stochastic texture sampling ───────────────────────────────────────────────

vec4 stochasticSample(sampler2D tex, vec2 uv) {
    vec2 skewed = uv + 0.36602540378 * (uv.x + uv.y);
    vec2 i = floor(skewed), f = fract(skewed);
    vec2  va, vb, vc; float wa, wb, wc;
    if (f.x > f.y) {
        va = i;               wa = 1.0 - f.x;
        vb = i + vec2(1,0);   wb = f.x - f.y;
        vc = i + vec2(1,1);   wc = f.y;
    } else {
        va = i;               wa = 1.0 - f.y;
        vb = i + vec2(0,1);   wb = f.y - f.x;
        vc = i + vec2(1,1);   wc = f.x;
    }
    return wa * texture(tex, uv + hash2(va))
         + wb * texture(tex, uv + hash2(vb))
         + wc * texture(tex, uv + hash2(vc));
}

// ── Terrain sample struct ─────────────────────────────────────────────────────

struct TerrainSample {
    vec3  color;
    vec3  normal;    // tangent-space
    float ao;        // 1.0 = fully lit, 0.0 = fully occluded
    float roughness; // 0.0 = smooth, 1.0 = rough
};

// Sample one material set.  AO + roughness use plain texture() (single sample
// is enough for greyscale maps; saves 4 extra stochastic lookups).
TerrainSample sampleMaterial(
    sampler2D diffTex, sampler2D norTex,
    sampler2D aoTex,    bool useAO,
    sampler2D roughTex, bool useRough,
    vec2 uv)
{
    TerrainSample s;
    s.color     = stochasticSample(diffTex, uv).rgb;
    s.normal    = stochasticSample(norTex,  uv).rgb * 2.0 - 1.0;
    // Clamp AO: never let it go below 0.5 so crack areas don't crush to black
    s.ao        = 1.0;  if (useAO)    s.ao        = max(texture(aoTex,    uv).r, 0.5);
    s.roughness = 0.8;  if (useRough) s.roughness = texture(roughTex, uv).r;
    return s;
}

// ── Per-axis samplers ─────────────────────────────────────────────────────────

// Top face: world-space UVs so texture flows across tile boundaries (no stamp).
TerrainSample sampleTop(vec3 worldPos) {
    vec2 uv = worldPos.xz * texTiling;
    TerrainSample s;
    // All terrain types use bark (type 0 used floor_tiles_16 which is a
    // top-down water texture — looks grey under toon shading).
    // floor_tiles_16 is kept wired in render_manager for future use.
    s = sampleMaterial(tex_grass_diff, tex_grass_nor,
                       tex_grass_ao, hasGrassAO,
                       tex_grass_rough, hasGrassRough, uv);
    if (iTerrainType == 3) s.color *= vec3(0.7, 0.85, 0.6);
    return s;
}

// Side / cliff faces.
TerrainSample sampleSide(vec2 uv) {
    return sampleMaterial(tex_rock2_diff, tex_rock2_nor,
                          tex_grass_ao, hasGrassAO,
                          tex_grass_rough, hasGrassRough,
                          uv * texTiling * 4.0);
}

// ── Triplanar ─────────────────────────────────────────────────────────────────

struct TriplanarResult {
    vec3  color;
    vec3  worldNormal;
    float ao;
    float roughness;
};

TriplanarResult getTerrainTriplanar(vec3 worldPos, vec3 geomNormal) {
    // Blend weights: sharp at face centres, soft at edges
    vec3 w = abs(geomNormal);
    w = pow(w, vec3(12.0));   // sharper blend — reduces side-face streak bleed
    w /= (w.x + w.y + w.z);

    TerrainSample xS = sampleSide(worldPos.yz);
    TerrainSample yS = sampleTop(worldPos);
    TerrainSample zS = sampleSide(worldPos.xy);

    // ── Colour blend ────────────────────────────────────────────────────────
    vec3 color = xS.color * w.x + yS.color * w.y + zS.color * w.z;

    // Macro FBM variation: subtle brightness patches (narrower range = no black pits)
    color *= mix(0.93, 1.07, fbm(worldPos.xz * 0.08));

    // Subtle edge darkening at tile borders (reduced to avoid dark grid)
    float minEdge = min(0.5 - abs(fs_in.LocalPos.x), 0.5 - abs(fs_in.LocalPos.z));
    color *= 0.96 + 0.04 * smoothstep(0.0, 0.06, minEdge);

    // ── Per-axis world-space normal reconstruction ────────────────────────
    // For each triplanar projection we know the UV axes in world space, so
    // we can map tangent-space (R,G,B) directly to world XYZ without the
    // mesh TBN.  Sign of the "depth" component follows the geometry normal.
    vec3 axisSign = sign(geomNormal);

    // X projection  (yz UVs → u=worldY, v=worldZ)
    //   tangent-space .x → worldY,  .y → worldZ,  .z → worldX (signed)
    vec3 wNX = vec3(xS.normal.z * axisSign.x, xS.normal.x, xS.normal.y);

    // Y projection  (xz UVs → u=worldX, v=worldZ)
    //   tangent-space .x → worldX,  .y → worldZ,  .z → worldY (signed)
    vec3 wNY = vec3(yS.normal.x, yS.normal.z * axisSign.y, yS.normal.y);

    // Z projection  (xy UVs → u=worldX, v=worldY)
    //   tangent-space .x → worldX,  .y → worldY,  .z → worldZ (signed)
    vec3 wNZ = vec3(zS.normal.x, zS.normal.y, zS.normal.z * axisSign.z);

    vec3 worldNormal = normalize(wNX * w.x + wNY * w.y + wNZ * w.z);

    // ── AO + roughness blend ─────────────────────────────────────────────
    float ao        = xS.ao * w.x + yS.ao * w.y + zS.ao * w.z;
    float roughness = xS.roughness * w.x + yS.roughness * w.y + zS.roughness * w.z;

    TriplanarResult r;
    r.color      = color;
    r.worldNormal = worldNormal;
    r.ao          = ao;
    r.roughness   = roughness;
    return r;
}

// ── Shadow ────────────────────────────────────────────────────────────────────

float ShadowCalculation(vec4 fragPosLightSpace, vec3 lightPos, vec3 normal) {
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float bias = max(0.002 * (1.0 - dot(normal, lightDir)), 0.0002);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float d = texture(shadowMap, proj.xy + vec2(x,y) * texelSize).r;
            shadow += proj.z - bias > d ? 1.0 : 0.0;
        }
    shadow /= 9.0;
    if (proj.z > 1.0) shadow = 0.0;
    return shadow;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    vec3 geomNormal = normalize(fs_in.Normal);
    vec3 viewDir    = normalize(viewPos - fs_in.FragPos);

    TriplanarResult terrain = getTerrainTriplanar(fs_in.FragPos, geomNormal);
    vec3  col       = terrain.color;
    vec3  normal    = terrain.worldNormal;
    float ao        = terrain.ao;
    float roughness = terrain.roughness;

    // AO darkens ambient; lifted floor from C++ side (max 0.5) + mix keeps it visible
    vec3 ambient = 0.20 * col * mix(ao, 1.0, 0.4);

    // Roughness → specular threshold: rough surface = threshold pushed to 1 (no spec)
    float specThresh = mix(0.98, SPEC_THRESHOLD, 1.0 - roughness);

    vec3 totalLighting = vec3(0.0);
    for (int i = 0; i < numLights; i++) {
        vec3  lightDir = normalize(lights[i].Position - fs_in.FragPos);

        // Cell-shaded diffuse
        float diff     = max(dot(lightDir, normal), 0.0);
        float cellDiff = floor(diff * float(LEVELS)) / float(LEVELS);
        float soft     = smoothstep(0.0, 0.1, diff - cellDiff);
        cellDiff = mix(cellDiff, cellDiff + 1.0 / float(LEVELS), soft * 0.3);

        // Cell-shaded specular gated by roughness
        vec3  H        = normalize(lightDir + viewDir);
        float spec     = pow(max(dot(normal, H), 0.0), 64.0);
        float cellSpec = smoothstep(specThresh - 0.05, specThresh + 0.05, spec);

        // Shadow (first light only)
        float shadow = (i == 0)
            ? smoothstep(0.4, 0.6, ShadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, normal))
            : 0.0;

        // AO also slightly darkens diffuse (mix so it isn't too crushed)
        vec3 diffuse  = cellDiff * lights[i].Color * col * mix(ao, 1.0, 0.4);
        vec3 specular = cellSpec * lights[i].Color * 0.25;
        totalLighting += mix(diffuse + specular, vec3(0.0), shadow);
    }

    // Rim
    float rimDot  = 1.0 - max(dot(viewDir, normal), 0.0);
    float rimI    = smoothstep(RIM_THRESHOLD, 1.0, pow(rimDot, RIM_POWER));
    vec3  rim     = vec3(0.4, 0.7, 0.9) * rimI * 0.4;

    vec3 finalColor = ambient + totalLighting + rim;

    // Saturation boost
    float lum = dot(finalColor, vec3(0.299, 0.587, 0.114));
    finalColor = mix(vec3(lum), finalColor, 1.15);

    FragColor = vec4(finalColor, 1.0);
}
