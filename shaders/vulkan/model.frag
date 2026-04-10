#version 450

// ── Inputs from vertex shader ───────────────────────────────────────────────
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// ── Output ──────────────────────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

// ── Descriptors ─────────────────────────────────────────────────────────────
struct PointLight {
    vec4 position;   // xyz = world pos
    vec4 color;      // xyz = RGB, w = intensity
};

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 lightPos;       // xyz = position, w = intensity
    vec4 viewPos;        // xyz = camera position, w = ambient intensity
    PointLight pointLights[16];
    int  numPointLights;
} frame;

layout(set = 0, binding = 1) uniform sampler2D diffuseTexture;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;  // manual comparison (MoltenVK)

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 albedoTint;
} push;

// ── Shadow calculation (3x3 PCF, manual depth comparison) ───────────────────
float calcShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // XY: clip space [-1,1] to UV [0,1]
    // Z: already [0,1] from adjusted ortho projection (done on CPU side)
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Outside far plane — no shadow
    if (projCoords.z > 1.0)
        return 0.0;

    // Dynamic bias based on surface angle to light
    float bias = max(0.002 * (1.0 - dot(normal, lightDir)), 0.0002);

    // 3x3 PCF kernel with manual depth comparison
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float storedDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > storedDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow; // 0.0 = fully lit, 1.0 = fully shadowed
}

void main() {
    vec3 lightDir = normalize(frame.lightPos.xyz - fragWorldPos);
    vec3 normal   = normalize(fragNormal);
    vec3 texColor = texture(diffuseTexture, fragTexCoord).rgb * push.albedoTint.rgb;

    // Shadow
    vec4 fragPosLightSpace = frame.lightSpaceMatrix * vec4(fragWorldPos, 1.0);
    float shadow = calcShadow(fragPosLightSpace, normal, lightDir);

    vec3 lighting = vec3(frame.viewPos.w);

    if (frame.lightPos.w > 0.0) {
        float diff = max(dot(normal, lightDir), 0.0);
        lighting += vec3((1.0 - shadow) * diff * (0.85 * frame.lightPos.w));
    }

    for (int i = 0; i < frame.numPointLights; i++) {
        vec3 lightVector = frame.pointLights[i].position.xyz - fragWorldPos;
        float distanceSq = max(dot(lightVector, lightVector), 0.25);
        vec3 pointDir = normalize(lightVector);
        float diffuse = max(dot(normal, pointDir), 0.0);
        vec3 pointColor = frame.pointLights[i].color.xyz * frame.pointLights[i].color.w;
        lighting += (pointColor / distanceSq) * diffuse;
    }

    outColor = vec4(texColor * lighting, 1.0);
}
