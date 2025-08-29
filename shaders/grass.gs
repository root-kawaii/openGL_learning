#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 8) out;  // Increased for better shape

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float windSpeed;
uniform float windStrength;
uniform float grassHeight;
uniform float grassWidth;

in vec3 WorldPos[];

out vec2 FragTexCoord;
out vec3 FragWorldPos;
out float GrassHeight;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main()
{
    vec3 worldPos = WorldPos[0];
    
    // Add randomness
    float grassRandom = random(worldPos.xz);
    float grassRandom2 = random(worldPos.xz + vec2(1.0, 1.0));
    
    // More varied height and width
    float heightVariation = grassHeight * (0.7 + grassRandom * 0.6);
    float widthVariation = grassWidth * (0.8 + grassRandom2 * 0.4);
    
    // Gentler wind that affects more at the top
    float windTime = time * windSpeed;
    float windX = sin(windTime + worldPos.x * 0.1 + grassRandom * 6.28) * windStrength;
    float windZ = cos(windTime * 0.7 + worldPos.z * 0.1 + grassRandom2 * 6.28) * windStrength;
    
    // Camera facing
    vec3 cameraPos = inverse(view)[3].xyz;
    vec3 toCamera = normalize(cameraPos - worldPos);
    vec3 up = vec3(0, 1, 0);
    vec3 right = normalize(cross(up, toCamera)) * widthVariation;
    
    // Create more realistic blade shape with multiple segments
    vec3 basePos = worldPos;
    vec3 midPos = basePos + vec3(0, heightVariation * 0.6, 0) + vec3(windX * 0.3, 0, windZ * 0.3);
    vec3 topPos = basePos + vec3(0, heightVariation, 0) + vec3(windX, 0, windZ);
    
    mat4 viewProj = projection * view;
    
    // Create a more realistic grass blade with curved shape
    
    // Bottom triangle (wide base)
    gl_Position = viewProj * vec4(basePos - right, 1.0);
    FragTexCoord = vec2(0.0, 0.0);
    FragWorldPos = basePos - right;
    GrassHeight = 0.0;
    EmitVertex();
    
    gl_Position = viewProj * vec4(basePos + right, 1.0);
    FragTexCoord = vec2(1.0, 0.0);
    FragWorldPos = basePos + right;
    GrassHeight = 0.0;
    EmitVertex();
    
    // Middle section (narrower)
    vec3 rightMid = right * 0.7;
    gl_Position = viewProj * vec4(midPos - rightMid, 1.0);
    FragTexCoord = vec2(0.15, 0.6);
    FragWorldPos = midPos - rightMid;
    GrassHeight = 0.6;
    EmitVertex();
    
    gl_Position = viewProj * vec4(midPos + rightMid, 1.0);
    FragTexCoord = vec2(0.85, 0.6);
    FragWorldPos = midPos + rightMid;
    GrassHeight = 0.6;
    EmitVertex();
    
    // Top (pointed)
    vec3 rightTop = right * 0.3;
    gl_Position = viewProj * vec4(topPos - rightTop, 1.0);
    FragTexCoord = vec2(0.3, 1.0);
    FragWorldPos = topPos - rightTop;
    GrassHeight = 1.0;
    EmitVertex();
    
    gl_Position = viewProj * vec4(topPos + rightTop, 1.0);
    FragTexCoord = vec2(0.7, 1.0);
    FragWorldPos = topPos + rightTop;
    GrassHeight = 1.0;
    EmitVertex();
    
    // Final tip
    gl_Position = viewProj * vec4(topPos, 1.0);
    FragTexCoord = vec2(0.5, 1.0);
    FragWorldPos = topPos;
    GrassHeight = 1.0;
    EmitVertex();
    
    gl_Position = viewProj * vec4(topPos, 1.0);
    FragTexCoord = vec2(0.5, 1.0);
    FragWorldPos = topPos;
    GrassHeight = 1.0;
    EmitVertex();
    
    EndPrimitive();
}