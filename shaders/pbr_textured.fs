// =============================================================================
//  pbr_textured.fs  —  Step 1b: identical BRDF to pbr_direct, texture-driven
//
//  The only difference from pbr_direct.fs is WHERE the material values come
//  from.  Instead of uniform floats/vecs set once per draw call, every pixel
//  reads its own albedo/metallic/roughness/ao from texture maps.
//
//  Two important details about texture colour spaces:
//    - albedoMap is authored in sRGB (gamma ~2.2).  We must linearise it
//      with pow(sample, 2.2) before using it in lighting math.
//    - normalMap, metallicMap, roughnessMap, aoMap are authored in linear
//      space.  Do NOT linearise them — use the raw sampled values.
//
//  The BRDF functions (D, G, F) and the main lighting loop are identical to
//  pbr_direct.fs.  Compare the two files side-by-side to see the difference.
// =============================================================================
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

uniform sampler2D albedoMap;     // sRGB — base colour
uniform sampler2D normalMap;     // linear — tangent-space normal (R=X, G=Y, B=Z mapped to [-1,1])
uniform sampler2D metallicMap;   // linear — single channel, 0=dielectric 1=metal
uniform sampler2D roughnessMap;  // linear — single channel, 0=smooth 1=rough
uniform sampler2D aoMap;         // linear — single channel, 0=occluded 1=lit

// ── 3. LIGHTS AND CAMERA ─────────────────────────────────────────────────────

struct Light { vec3 Position; vec3 Color; };
#define MAX_LIGHTS 8
uniform Light       lights[MAX_LIGHTS];
uniform int         numLights;
uniform vec3        viewPos;
uniform sampler2D   shadowMap;
uniform samplerCube envMap;          // skybox cubemap for environment reflections
uniform float       texTiling = 4.0; // how many times the texture repeats per tile

// ── 4. PBR MATH FUNCTIONS — identical to pbr_direct.fs ───────────────────────

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
    return geometrySchlickGGX(NdotV, roughness)
         * geometrySchlickGGX(NdotL, roughness);
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

// ── 5. MAIN ───────────────────────────────────────────────────────────────────

void main()
{
    // ── 5a. Read per-pixel material values from textures ──────────────────────

    // albedo is sRGB → convert to linear before any lighting math
    vec2  uv        = fs_in.TexCoords * texTiling;
    vec3  albedo    = pow(texture(albedoMap, uv).rgb, vec3(2.2));

    // Normal map: tangent-space vector packed into [0,1] → remap to [-1,1]
    vec3  tangentNormal = texture(normalMap, uv).rgb * 2.0 - 1.0;
    // Fallback to geometric normal when no normal map is bound (default texture
    // returns ~(0,0,0) → tangentNormal.z ≈ -1 after remap, which is implausible).
    vec3  N = (tangentNormal.z > 0.1)
            ? normalize(fs_in.TBN * tangentNormal)
            : normalize(fs_in.Normal);

    // ARM-packed textures store AO(R) Roughness(G) Metallic(B).
    // Grayscale separate maps have R==G==B so reading these channels is safe.
    float metallic  = texture(metallicMap,  uv).b;
    float roughness = texture(roughnessMap, uv).g;
    float ao        = texture(aoMap,        uv).r;

    // ── 5b. Camera direction ──────────────────────────────────────────────────
    vec3 V = normalize(viewPos - fs_in.WorldPos);

    // F0: 0.04 for dielectrics; for metals the albedo itself tints reflections
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── 5c. Reflectance equation — same loop as pbr_direct.fs ─────────────────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < numLights; i++)
    {
        vec3  L    = normalize(lights[i].Position - fs_in.WorldPos);
        vec3  H    = normalize(V + L);

        float dist        = length(lights[i].Position - fs_in.WorldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3  radiance    = lights[i].Color * attenuation;

        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  specular = (D * G * F)
                       / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float shadow = (i == 0)
            ? shadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, N)
            : 0.0;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    // ── 5d. Environment reflection ────────────────────────────────────────────
    // Reflect the view ray off the surface normal, then sample the skybox.
    // textureLod blurs the reflection based on roughness (high lod = blurry = rough).
    vec3  R        = reflect(-V, N);
    float maxLOD   = 7.0;
    vec3  envColor = textureLod(envMap, R, roughness * maxLOD).rgb;
    envColor = pow(envColor, vec3(2.2)); // skybox faces are sRGB JPEGs — linearise for PBR math

    // Fresnel at the view angle drives how much env light reflects off the surface.
    // Metals tint the reflection with their albedo; dielectrics reflect white at grazing.
    vec3 kS_env     = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 envSpecular = kS_env * envColor * (1.0 - roughness);  // rough surfaces scatter env light

    // Ambient: diffuse AO term + env specular
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo + envSpecular;

    // ── 5e. Tone mapping + gamma correction ───────────────────────────────────
    color = color / (color + vec3(1.0));   // Reinhard
    color = pow(color, vec3(1.0 / 2.2));   // linear → sRGB

    FragColor = vec4(color, 1.0);
}
