#version 450

// ── Inputs ─────────────────────────────────────────────────────────────────
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragPosLightSpace;
layout(location = 4) in mat3 fragTBN;   // locations 4,5,6

// ── Output ─────────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Descriptors ────────────────────────────────────────────────────────────
struct PointLight {
    vec4 position;   // xyz = world pos
    vec4 color;      // xyz = RGB, w = intensity
};

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4       view;
    mat4       proj;
    mat4       lightSpaceMatrix;
    vec4       lightPos;
    vec4       viewPos;
    PointLight pointLights[16];
    int        numPointLights;
} frame;

layout(set = 0, binding = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D metallicMap;
layout(set = 0, binding = 4) uniform sampler2D roughnessMap;
layout(set = 0, binding = 5) uniform sampler2D shadowMap;

// ── Push constant ──────────────────────────────────────────────────────────
layout(push_constant) uniform PBRPush {
    mat4  model;
    float metallicVal;
    float roughnessVal;
    uint  hasNormalMap;
    uint  _pad;
} push;

// ── PBR math ───────────────────────────────────────────────────────────────
const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = pow(max(dot(N, H), 0.0), 2.0);
    float denom  = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// Evaluates the Cook-Torrance BRDF for one light contribution (no shadow)
vec3 evalLight(vec3 N, vec3 V, vec3 F0, vec3 albedo,
               float metallic, float roughness,
               vec3 lightPos, vec3 lightColor) {
    vec3  L        = normalize(lightPos - fragWorldPos);
    vec3  H        = normalize(V + L);
    float dist     = length(lightPos - fragWorldPos);
    vec3  radiance = lightColor / (dist * dist + 0.01);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F)
                  / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    return (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0);
}

float calcShadow(vec4 fragPosLS, vec3 N, vec3 lightDir) {
    vec3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.xy   = proj.xy * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float bias      = max(0.002 * (1.0 - dot(N, lightDir)), 0.0002);
    float shadow    = 0.0;
    vec2  texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float depth = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += (proj.z - bias > depth) ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

void main() {
    // Albedo — linearize from sRGB
    vec3 albedo = pow(texture(albedoMap, fragTexCoord).rgb, vec3(2.2));

    // Normal
    vec3 N;
    if (push.hasNormalMap != 0u) {
        vec3 tn = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
        N = (tn.z > 0.1) ? normalize(fragTBN * tn) : normalize(fragNormal);
    } else {
        N = normalize(fragNormal);
    }

    // Metallic / roughness from textures, scaled by push constant fallback
    float metallic  = clamp(texture(metallicMap,  fragTexCoord).r * push.metallicVal,  0.0,  1.0);
    float roughness = clamp(texture(roughnessMap, fragTexCoord).r * push.roughnessVal, 0.04, 1.0);

    vec3 V  = normalize(frame.viewPos.xyz - fragWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // ── Shadow-casting key light ──────────────────────────────────────────
    {
        vec3 L      = normalize(frame.lightPos.xyz - fragWorldPos);
        float shadow = calcShadow(fragPosLightSpace, N, L);
        Lo += evalLight(N, V, F0, albedo, metallic, roughness,
                        frame.lightPos.xyz, vec3(8.0 * frame.lightPos.w)) * (1.0 - shadow);
    }

    // ── Extra point lights ────────────────────────────────────────────────
    for (int i = 0; i < frame.numPointLights; i++) {
        vec3 lPos   = frame.pointLights[i].position.xyz;
        vec3 lColor = frame.pointLights[i].color.xyz
                    * frame.pointLights[i].color.w;  // w = intensity
        Lo += evalLight(N, V, F0, albedo, metallic, roughness, lPos, lColor);
    }

    vec3 ambient = vec3(frame.viewPos.w) * albedo;
    vec3 color   = ambient + Lo;

    // Reinhard tone map + gamma encode
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
