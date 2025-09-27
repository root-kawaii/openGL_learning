#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 32) out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float grassHeight;
uniform float grassWidth;
uniform float windSpeed;
uniform float windStrength;
uniform vec2 windFrequency;
uniform float bladeHeightRandom;
uniform float bladeWidthRandom;
uniform float bendRotationRandom;
uniform float bladeForward;
uniform float bladeCurve;
uniform vec3 cameraPos;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec3 WorldPos;

// Hash function for better randomness
float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

// Smooth noise function
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash2(i);
    float b = hash2(i + vec2(1.0, 0.0));
    float c = hash2(i + vec2(0.0, 1.0));
    float d = hash2(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Generate wind displacement
vec3 getWind(vec3 worldPos, float time) {
    vec2 uv = worldPos.xz * windFrequency;
    float windNoise = noise(uv + time * windSpeed) * 0.5 + 0.5;
    windNoise += noise(uv * 2.0 + time * windSpeed * 1.2) * 0.25;
    windNoise += noise(uv * 4.0 + time * windSpeed * 0.8) * 0.125;
    
    vec3 wind = vec3(
        sin(time * windSpeed + worldPos.x * 0.1) * windNoise,
        0.0,
        cos(time * windSpeed + worldPos.z * 0.1) * windNoise
    );
    
    return wind * windStrength;
}

void main() {
    vec4 worldPos = model * gl_in[0].gl_Position;
    vec3 basePos = worldPos.xyz;
    
    // Generate random values based on position
    float rand1 = hash2(basePos.xz);
    float rand2 = hash2(basePos.xz + vec2(12.34, 56.78));
    float rand3 = hash2(basePos.xz + vec2(91.01, 23.45));
    float rand4 = hash2(basePos.xz + vec2(67.89, 10.11));
    
    // Randomize grass blade properties
    float height = grassHeight * (1.0 + (rand1 - 0.5) * bladeHeightRandom);
    float width = grassWidth * (1.0 + (rand2 - 0.5) * bladeWidthRandom);
    float rotation = (rand3 - 0.5) * bendRotationRandom * 6.28318; // 2*PI
    float forward = bladeForward * (0.5 + rand4 * 0.5);
    
    // Create rotation matrix
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    mat3 rotMat = mat3(
        cosR, 0.0, sinR,
        0.0, 1.0, 0.0,
        -sinR, 0.0, cosR
    );
    
    // Calculate camera direction for billboarding
    vec3 camDir = normalize(cameraPos - basePos);
    vec3 right = normalize(cross(vec3(0, 1, 0), camDir));
    vec3 up = vec3(0, 1, 0);
    
    // Generate grass blade with multiple segments for smooth curves
    int segments = 8; // Number of segments along the blade height
    float segmentHeight = height / float(segments);
    
    for (int i = 0; i <= segments; i++) {
        float t = float(i) / float(segments);
        float currentHeight = t * height;
        
        // Apply curve (quadratic for natural bend)
        float curve = pow(t, bladeCurve) * forward;
        
        // Calculate wind effect (stronger at the top)
        vec3 windOffset = getWind(basePos, time) * t * t;
        
        // Width tapering (wider at base, narrower at tip)
        float currentWidth = width * (1.0 - t * 0.7);
        
        // Current position along the blade
        vec3 segmentPos = basePos + vec3(curve, currentHeight, 0.0) + windOffset;
        segmentPos = basePos + rotMat * (segmentPos - basePos);
        
        // Generate quad vertices for this segment
        vec3 leftPos = segmentPos - right * currentWidth * 0.5;
        vec3 rightPos = segmentPos + right * currentWidth * 0.5;
        
        // Transform to clip space
        vec4 leftClip = projection * view * vec4(leftPos, 1.0);
        vec4 rightClip = projection * view * vec4(rightPos, 1.0);
        
        // Calculate normals (facing camera for better lighting)
        vec3 normal = normalize(cross(right, up));
        
        // Left vertex
        gl_Position = leftClip;
        FragPos = leftPos;
        Normal = normal;
        TexCoord = vec2(0.0, t);
        WorldPos = leftPos;
        EmitVertex();
        
        // Right vertex
        gl_Position = rightClip;
        FragPos = rightPos;
        Normal = normal;
        TexCoord = vec2(1.0, t);
        WorldPos = rightPos;
        EmitVertex();
    }
    
    EndPrimitive();
    
    // Optional: Generate a second blade with different properties for more density
    if (rand1 > 0.3) { // 70% chance for second blade
        float height2 = grassHeight * 0.8 * (1.0 + (rand2 - 0.5) * bladeHeightRandom);
        float width2 = grassWidth * 0.7 * (1.0 + (rand3 - 0.5) * bladeWidthRandom);
        float rotation2 = rotation + (rand4 - 0.5) * 1.57; // +/- 90 degrees
        
        // Second blade rotation
        float cosR2 = cos(rotation2);
        float sinR2 = sin(rotation2);
        mat3 rotMat2 = mat3(
            cosR2, 0.0, sinR2,
            0.0, 1.0, 0.0,
            -sinR2, 0.0, cosR2
        );
        
        for (int i = 0; i <= segments; i++) {
            float t = float(i) / float(segments);
            float currentHeight = t * height2;
            float curve = pow(t, bladeCurve * 1.2) * forward * 0.8;
            vec3 windOffset = getWind(basePos + vec3(0.1, 0, 0.1), time) * t * t;
            float currentWidth = width2 * (1.0 - t * 0.8);
            
            vec3 segmentPos = basePos + vec3(curve, currentHeight, 0.0) + windOffset;
            segmentPos = basePos + rotMat2 * (segmentPos - basePos);
            
            vec3 leftPos = segmentPos - right * currentWidth * 0.5;
            vec3 rightPos = segmentPos + right * currentWidth * 0.5;
            
            vec4 leftClip = projection * view * vec4(leftPos, 1.0);
            vec4 rightClip = projection * view * vec4(rightPos, 1.0);
            
            vec3 normal = normalize(cross(right, up));
            
            gl_Position = leftClip;
            FragPos = leftPos;
            Normal = normal;
            TexCoord = vec2(0.0, t);
            WorldPos = leftPos;
            EmitVertex();
            
            gl_Position = rightClip;
            FragPos = rightPos;
            Normal = normal;
            TexCoord = vec2(1.0, t);
            WorldPos = rightPos;
            EmitVertex();
        }
        
        EndPrimitive();
    }
}