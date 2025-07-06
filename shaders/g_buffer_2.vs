#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out float viewDepth;
out float elevation;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float seed; // Seed for randomness - change this to get different terrain

// Hard-coded terrain parameters
const float mountainHeight = 15.0;
const float terrainScale = 0.05;
const float ridgeSharpness = 0.75;
const float valleyDepth = 0.1;
const float noiseDetailScale = 0.25;
const float detailStrength = 0.05;

// Better seed hashing function
vec3 hash3(float seed) {
    vec3 p = vec3(seed * 127.1, seed * 311.7, seed * 74.7);
    return fract(sin(p) * 43758.5453123);
}

// Perlin noise implementation
vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
    return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
    
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);
    
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;
    
    i = mod289(i);
    vec4 p = permute(permute(permute(
        i.z + vec4(0.0, i1.z, i2.z, 1.0))
        + i.y + vec4(0.0, i1.y, i2.y, 1.0))
        + i.x + vec4(0.0, i1.x, i2.x, 1.0));
    
    float n_ = 0.142857142857;
    vec3 ns = n_ * D.wyz - D.xzx;
    
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    
    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    
    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
    
    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);
    
    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for(int i = 0; i < 5; i++) {
        value += amplitude * snoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

float ridgeNoise(vec3 p) {
    return 1.0 - abs(snoise(p));
}

float ridgeFbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for(int i = 0; i < 4; i++) {
        value += amplitude * ridgeNoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Improved terrain height function with better seed distribution
float getTerrainHeight(vec2 pos) {
    vec3 p = vec3(pos.x, 0.0, pos.y);
    
    // Create much more varied seed offsets using hash function
    vec3 seedOffset1 = hash3(seed) * 1000.0;
    vec3 seedOffset2 = hash3(seed + 17.3) * 1000.0;
    vec3 seedOffset3 = hash3(seed + 39.7) * 1000.0;
    vec3 seedOffset4 = hash3(seed + 67.2) * 1000.0;
    vec3 seedOffset5 = hash3(seed + 91.8) * 1000.0;
    
    // Base mountain shape with ridges
    float ridges = ridgeFbm((p + seedOffset1) * terrainScale * 0.5) * ridgeSharpness;
    
    // Large scale mountain formations
    float mountains = fbm((p + seedOffset2) * terrainScale * 0.3) * 0.8;
    
    // Medium scale hills and valleys
    float hills = fbm((p + seedOffset3) * terrainScale * 0.8) * 0.6;
    
    // Small scale detail
    float detail = fbm((p + seedOffset4) * noiseDetailScale) * detailStrength;
    
    // Combine different scales
    float baseHeight = mountains + hills * 0.7;
    float finalHeight = baseHeight + ridges * pow(max(0.0, baseHeight + 0.5), 2.0);
    
    // Add fine detail
    finalHeight += detail * (1.0 + finalHeight * 0.5);
    
    // Create valleys by subtracting inverted noise
    float valleys = fbm((p + seedOffset5) * terrainScale * 1.2) * valleyDepth;
    finalHeight -= valleys * valleys;
    
    return finalHeight * mountainHeight;
}

void main()
{
    vec3 pos = aPos;
    
    // Generate terrain height
    float terrainHeight = getTerrainHeight(pos.xz);
    pos.y += terrainHeight;
    
    // Calculate world position
    vec4 worldPos = model * vec4(pos, 1.0);
    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;
    
    // Calculate normals using finite differences
    float epsilon = 0.1;
    float hL = getTerrainHeight(pos.xz + vec2(-epsilon, 0.0)); // fix this i think
    float hR = getTerrainHeight(pos.xz + vec2(epsilon, 0.0));
    float hD = getTerrainHeight(pos.xz + vec2(0.0, -epsilon));
    float hU = getTerrainHeight(pos.xz + vec2(0.0, epsilon));
    
    vec3 tangentX = normalize(vec3(2.0 * epsilon, hR - hL, 0.0));
    vec3 tangentZ = normalize(vec3(0.0, hU - hD, 2.0 * epsilon));
    vec3 terrainNormal = normalize(cross(tangentX, tangentZ));
    
    // Transform normal to world space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalMatrix * terrainNormal;
    
    // Calculate view depth
    vec3 viewPos = (view * model * vec4(pos, 1.0)).xyz;
    viewDepth = -viewPos.z;
    
    // Pass elevation for fragment shader
    elevation = clamp((terrainHeight + mountainHeight * 0.2) / (mountainHeight * 1.4), 0.0, 1.0);
    
    gl_Position = projection * view * worldPos;
}