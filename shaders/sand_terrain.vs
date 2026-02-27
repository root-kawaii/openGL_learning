#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform float windAngle;
uniform float time;
uniform vec4 clipPlane;

// --- Helper: Simple Noise ---
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// --- The Dune Math ---
float getDuneHeight(vec2 p) {
    // ADJUST THIS: If dunes are too big, increase 50.0. If too small, decrease it.
    float freqScale = 50.0; 
    vec2 uv = p * freqScale;

    // Rotate for Wind
    float wz = uv.x * sin(windAngle) + uv.y * cos(windAngle);
    float wx = uv.x * cos(windAngle) - uv.y * sin(windAngle);

    // 1. Create a "Warped" Ridge
    float ridgeWarp = noise(uv * 0.1) * 10.0;
    float val = sin(wz + ridgeWarp);

    // 2. Asymmetric Slope (From the GitHub logic)
    // This makes the "windward" side long and the "leeward" side sharp.
    float h = (val > 0.0) ? pow(val, 0.5) : -pow(abs(val), 3.0);
    
    // 3. Amplitude (Vertical Height)
    float height = h * 2.5; 

    // 4. Large scale terrain variation
    height += noise(uv * 0.05) * 5.0;

    return height;
}

vec3 calculateNormal(vec2 p) {
    float e = 0.005; // Small epsilon for detail
    float hL = getDuneHeight(vec2(p.x - e, p.y));
    float hR = getDuneHeight(vec2(p.x + e, p.y));
    float hD = getDuneHeight(vec2(p.x, p.y - e));
    float hU = getDuneHeight(vec2(p.x, p.y + e));
    return normalize(vec3(hL - hR, 2.0 * e, hD - hU));
}

void main() {
    vec3 pos = aPos;
    
    // Apply Displacement
    float h = getDuneHeight(pos.xz);
    pos.y += h;

    vec4 worldPos4 = model * vec4(pos, 1.0);
    gl_ClipDistance[0] = dot(worldPos4, clipPlane);
    vs_out.FragPos = worldPos4.xyz;
    
    // Normal calculation
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vs_out.Normal = normalize(normalMatrix * calculateNormal(aPos.xz));
    
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
    vs_out.boneIDs = aBoneIDs;
    vs_out.weights = aWeights;

    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}