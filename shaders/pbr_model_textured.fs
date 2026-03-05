// =============================================================================
//  pbr_model_textured.fs  —  PBR with embedded model textures
//
//  Identical Cook-Torrance BRDF to pbr_direct / pbr_textured, but material
//  values come from the model's OWN embedded textures rather than the named
//  material library.
//
//  Why a separate shader?
//  ──────────────────────
//  Mesh::Draw() binds the model's embedded textures starting at GL_TEXTURE0,
//  using uniform names "texture_diffuse1", "texture_normal1" etc.
//
//  The named material system (pbr_textured) also starts at slot 0, which means
//  the two systems collide: whichever binds last wins, and shadow map slot 0
//  gets overwritten, making everything fully shadowed (black).
//
//  This shader sidesteps the problem by:
//    • Reading albedo + normal from the names Mesh::Draw naturally writes
//      ("texture_diffuse1" at slot 0, "texture_normal1" at slot 1)
//    • Expecting shadowMap at slot 15 — bound by the C++ useShader block
//      AFTER the material textures, so Draw can never clobber it
//    • Using uniform defaults for metallic / roughness / ao (no maps needed)
//
//  Result: GLB/FBX drones with embedded PBR textures render correctly.
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

// ── 2. MATERIAL TEXTURES (names match what Mesh::Draw sets) ──────────────────

// Bound by Mesh::Draw at slot 0 — base colour in sRGB, must linearise
uniform sampler2D texture_diffuse1;

// Bound by Mesh::Draw at slot 1 — tangent-space normal map (if present)
// If the model has no normal map, slot 1 holds whatever was last bound;
// the shader detects a near-zero Z channel and falls back to the geometric normal.
uniform sampler2D texture_normal1;

// Uniform PBR scalars — expose these per-object later if needed
uniform float metallic  = 0.0;  // drones are painted metal: tune this up
uniform float roughness = 0.4;  // moderately smooth painted surface
uniform float ao        = 1.0;

// ── 3. LIGHTS, CAMERA, SHADOW ────────────────────────────────────────────────

struct Light { vec3 Position; vec3 Color; };
#define MAX_LIGHTS 8
uniform Light     lights[MAX_LIGHTS];
uniform int       numLights;
uniform vec3      viewPos;

// Bound at slot 15 by C++ useShader — safe from Mesh::Draw overwrite
uniform sampler2D shadowMap;
// Bound at slot 14 by C++ useShader — safe from Mesh::Draw overwrite
uniform samplerCube envMap;

// ── 4. PBR MATH (same as pbr_direct.fs) ──────────────────────────────────────

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = pow(max(dot(N, H), 0.0), 2.0);
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

// ── 5. MAIN ───────────────────────────────────────────────────────────────────

void main()
{
    // ── 5a. Albedo: linearise from sRGB (model textures are authored in sRGB) ─
    vec3 albedo = pow(texture(texture_diffuse1, fs_in.TexCoords).rgb, vec3(2.2));

    // ── 5b. Normal: use tangent-space map if present, else geometric normal ───
    vec3 tangentNormal = texture(texture_normal1, fs_in.TexCoords).rgb * 2.0 - 1.0;
    vec3 N;
    // If the model has no normal map the sampler returns a flat (0,0,1) which
    // after *2-1 gives (−1,−1,1) — fallback guard: use geometric normal
    // whenever the Z component looks implausible.
    if (tangentNormal.z > 0.1)
        N = normalize(fs_in.TBN * tangentNormal);
    else
        N = normalize(fs_in.Normal);

    vec3 V  = normalize(viewPos - fs_in.WorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── 5c. Reflectance accumulation ─────────────────────────────────────────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < numLights; i++)
    {
        vec3  L    = normalize(lights[i].Position - fs_in.WorldPos);
        vec3  H    = normalize(V + L);
        float dist = length(lights[i].Position - fs_in.WorldPos);
        vec3  radiance = lights[i].Color / (dist * dist);

        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  specular = (D * G * F)
                       / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);

        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        float shadow = (i == 0)
            ? shadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, N)
            : 0.0;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    vec3  R        = reflect(-V, N);
    vec3  envColor = textureLod(envMap, R, roughness * 7.0).rgb;
    envColor = pow(envColor, vec3(2.2)); // skybox faces are sRGB JPEGs — linearise for PBR math
    vec3  kS_env   = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3  envSpecular = kS_env * envColor * (1.0 - roughness);

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo * ao + envSpecular * ao;

    color = color / (color + vec3(1.0));        // Reinhard tone map
    color = pow(color, vec3(1.0 / 2.2));        // gamma encode

    FragColor = vec4(color, 1.0);
}
