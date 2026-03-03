// =============================================================================
//  pbr_direct.fs  —  Step 1: Cook-Torrance BRDF, direct analytic lights only
//
//  Material parameters come in as uniforms (single values for the whole mesh).
//  This is the cleanest way to understand the math before adding texture maps.
//
//  The reflectance equation we are solving for each light:
//
//    Lo(p, ωo) = Σ  [ kd · (albedo/π)  +  D·G·F / (4·NdotV·NdotL) ]
//                       └── diffuse ──┘   └─────── specular ─────────┘
//               ·  Li(p, ωi) · NdotL
//
//  Terms:
//    D  = GGX  Normal Distribution Function  → how many microfacets face H
//    G  = Smith Geometry Function            → microfacet self-shadowing
//    F  = Fresnel-Schlick                    → reflected vs refracted fraction
//    kd = (1 - F) * (1 - metallic)          → diffuse contribution weight
//    Li = light radiance (color * attenuation)
//    NdotL = cos of angle between surface normal and light direction
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

// ── 2. MATERIAL PARAMETERS ───────────────────────────────────────────────────
// All in linear space. For dielectrics (stone, wood, plastic) metallic = 0.
// For conductors (gold, copper, iron) metallic = 1 and albedo tints the specular.

uniform vec3  albedo;     // base colour, linear space [0..1]
uniform float metallic;   // 0 = dielectric (plastic/stone), 1 = conductor (metal)
uniform float roughness;  // 0 = mirror-smooth, 1 = fully diffuse
uniform float ao;         // ambient occlusion [0..1], 1 = fully lit

// ── 3. LIGHTS AND CAMERA ─────────────────────────────────────────────────────
// Same struct the rest of the engine uses.

struct Light { vec3 Position; vec3 Color; };
#define MAX_LIGHTS 8
uniform Light       lights[MAX_LIGHTS];
uniform int         numLights;
uniform vec3        viewPos;
uniform sampler2D   shadowMap;

// ── 4. PBR MATH FUNCTIONS ─────────────────────────────────────────────────────

const float PI = 3.14159265359;

// ── 4a. Fresnel-Schlick ───────────────────────────────────────────────────────
// Answers: "what fraction of incoming light is reflected (vs refracted)?"
// At grazing angles (cosTheta → 0) all materials become perfect mirrors.
// F0 is the base reflectance when looking straight at the surface (0°):
//   dielectrics → F0 ≈ vec3(0.04)
//   metals      → F0 = albedo  (tinted reflections)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ── 4b. GGX Normal Distribution Function ─────────────────────────────────────
// Answers: "what fraction of microfacets are aligned with the halfway vector H?"
// This determines the size and shape of the specular highlight.
// Low roughness → sharp peak.  High roughness → wide, diffuse highlight.
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness; // Disney/Epic: square for perceptual linearity
    float a2 = a * a;

    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// ── 4c. Smith Geometry Function ───────────────────────────────────────────────
// Answers: "what fraction of microfacets are not shadowed by their neighbours?"
// We check self-shadowing from both the view direction (masking) and the
// light direction (shadowing), then multiply them — that is Smith's method.

// Single-direction term (Schlick-GGX approximation)
float geometrySchlickGGX(float NdotV, float roughness)
{
    // k is a remapping of roughness specific to direct lighting
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Combined Smith term: shadowing from viewer side AND light side
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float shadowFromViewer = geometrySchlickGGX(NdotV, roughness);
    float shadowFromLight  = geometrySchlickGGX(NdotL, roughness);
    return shadowFromViewer * shadowFromLight;
}

// ── 4d. Shadow map PCF ────────────────────────────────────────────────────────
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
    // ── 5a. Surface vectors ───────────────────────────────────────────────────
    vec3 N = normalize(fs_in.Normal);     // surface normal (world space)
    vec3 V = normalize(viewPos - fs_in.WorldPos); // direction toward camera

    // Base reflectance at 0° incidence.
    // Dielectrics: fixed 0.04 (measured average for glass/plastic/stone).
    // Metals: the albedo itself becomes the tinted F0.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── 5b. Reflectance equation: accumulate contribution from every light ─────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < numLights; i++)
    {
        // --- Per-light geometry ---
        vec3  L    = normalize(lights[i].Position - fs_in.WorldPos);
        vec3  H    = normalize(V + L);  // halfway vector

        // Inverse-square law: physically correct energy falloff
        float dist        = length(lights[i].Position - fs_in.WorldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3  radiance    = lights[i].Color * attenuation;

        // --- Cook-Torrance BRDF ---

        float D = distributionGGX(N, H, roughness); // microfacet peak
        float G = geometrySmith(N, V, L, roughness); // self-shadowing
        vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0); // Fresnel ratio

        // Specular fraction  =  D * G * F  /  (4 * NdotV * NdotL)
        vec3  specNumerator   = D * G * F;
        float specDenominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular        = specNumerator / specDenominator;

        // kS = reflected (= F).  kD = remaining refracted fraction.
        // Metals have no diffuse (all energy goes into tinted specular).
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        // Shadow on the first light (the one that casts shadows)
        float shadow = (i == 0)
            ? shadowCalculation(fs_in.FragPosLightSpace, lights[i].Position, N)
            : 0.0;

        float NdotL = max(dot(N, L), 0.0);

        // Sum: (diffuse + specular) * radiance * Lambert cosine * (1 - shadow)
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    // ── 5c. Ambient ───────────────────────────────────────────────────────────
    // Constant 0.03 is a placeholder for the environment's indirect lighting.
    // In Step 2 (IBL) this will be a lookup into a pre-convolved irradiance map,
    // making the ambient respond to the actual sky/environment colours.
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo;

    // ── 5d. Tone mapping ──────────────────────────────────────────────────────
    // PBR lighting works in unbounded HDR (Lo >> 1 is normal near a bright light).
    // Reinhard maps the full HDR range to [0,1] while preserving relative brightness.
    color = color / (color + vec3(1.0));

    // ── 5e. Gamma correction ──────────────────────────────────────────────────
    // All our lighting math was done in linear space.
    // Monitors expect gamma-encoded (sRGB ~2.2) output, so we apply the inverse.
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
