#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 7) out; // BLADE_SEGMENTS * 2 + 1 (3 segments)

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float windSpeed;
uniform float windStrength;
uniform float grassHeight;
uniform float grassWidth;

// New Unity-style uniforms
uniform float bladeHeightRandom;
uniform float bladeWidthRandom;
uniform float bendRotationRandom;
uniform float bladeForward;
uniform float bladeCurve;
uniform sampler2D windDistortionMap;
uniform vec2 windFrequency;
uniform sampler2D grassMask;
uniform float grassMaskThreshold;

in vec3 WorldPos[];

out vec2 FragTexCoord;
out vec3 FragWorldPos;
out float GrassHeight;
out vec3 FragNormal;

#define BLADE_SEGMENTS 6
#define PI 3.14159265359
#define TWO_PI 6.28318530718

// Unity-style random function
float rand(vec3 co) {
    return fract(sin(dot(co.xyz, vec3(12.9898, 78.233, 45.5432))) * 43758.5453);
}

// Create rotation matrix around axis
mat3 angleAxis3x3(float angle, vec3 axis) {
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return mat3(
        t * x * x + c,       t * x * y - s * z,   t * x * z + s * y,
        t * x * y + s * z,   t * y * y + c,       t * y * z - s * x,
        t * x * z - s * y,   t * y * z + s * x,   t * z * z + c
    );
}

// Generate a single grass vertex
void generateGrassVertex(vec3 vertexPosition, float width, float height, float forward, 
                        vec2 uv, mat3 transformMatrix, vec3 worldPos) {
    vec3 tangentPoint = vec3(width, forward, height);
    vec3 tangentNormal = normalize(vec3(0.0, -1.0, forward));
    vec3 localNormal = transformMatrix * tangentNormal;
    vec3 localPosition = vertexPosition + transformMatrix * tangentPoint;
    
    gl_Position = projection * view * vec4(localPosition, 1.0);
    FragTexCoord = uv;
    FragWorldPos = localPosition;
    GrassHeight = height;
    FragNormal = localNormal;
    
    EmitVertex();
}

void main() {
    vec3 pos = WorldPos[0];
    
    // Create tangent space (simplified - assuming Y-up world)
    vec3 vNormal = vec3(0, 1, 0);
    vec3 vTangent = vec3(1, 0, 0);
    vec3 vBinormal = cross(vNormal, vTangent);
    
    mat3 tangentToLocal = mat3(
        vTangent.x, vBinormal.x, vNormal.x,
        vTangent.y, vBinormal.y, vNormal.y,
        vTangent.z, vBinormal.z, vNormal.z
    );
    
    // Wind calculation using distortion map
    vec2 windUV = pos.xz * 0.1 + windFrequency * time;
    vec2 windSample = (texture(windDistortionMap, windUV).xy * 2.0 - 1.0) * windStrength;
    vec3 wind = normalize(vec3(windSample.x, windSample.y, 0.0));
    mat3 windRotation = angleAxis3x3(PI * length(windSample), wind);
    
    // Random rotations for variety
    mat3 facingRotationMatrix = angleAxis3x3(rand(pos) * TWO_PI, vec3(0, 0, 1));
    mat3 bendRotationMatrix = angleAxis3x3(rand(pos.zyx) * bendRotationRandom * PI * 0.5, vec3(-1, 0, 0));
    
    // Combine transformations
    mat3 transformationMatrix = tangentToLocal * windRotation * facingRotationMatrix * bendRotationMatrix;
    mat3 transformationMatrixFacing = tangentToLocal * facingRotationMatrix;
    
    // Sample grass mask
    vec2 maskUV = pos.xz * 0.1; // Adjust scale as needed
    float mask = texture(grassMask, maskUV).r;
    
    // Calculate blade properties with randomization
    float height = (rand(pos.zyx) * 2.0 - 1.0) * bladeHeightRandom + grassHeight;
    height *= mask;
    
    float width = (rand(pos.xzy) * 2.0 - 1.0) * bladeWidthRandom + grassWidth;
    width *= mask;
    
    float forward = rand(pos.yyz) * bladeForward;
    
    // Only generate grass if mask is above threshold
    if (mask <= grassMaskThreshold) {
        return;
    }
    
    // Generate grass blade segments
    for (int i = 0; i < BLADE_SEGMENTS; i++) {
        float t = float(i) / float(BLADE_SEGMENTS);
        
        float segmentHeight = height * t;
        float segmentWidth = width * (1.0 - t);
        float segmentForward = pow(t, bladeCurve) * forward;
        
        // Use facing matrix for base, full transformation for upper segments
        mat3 transformMatrix = (i == 0) ? transformationMatrixFacing : transformationMatrix;
        
        // Generate two vertices for this segment (left and right)
        generateGrassVertex(pos, segmentWidth, segmentHeight, segmentForward, 
                          vec2(0.0, t), transformMatrix, pos);
        generateGrassVertex(pos, -segmentWidth, segmentHeight, segmentForward, 
                          vec2(1.0, t), transformMatrix, pos);
    }
    
    // Generate tip vertex
    generateGrassVertex(pos, 0.0, height, forward, 
                       vec2(0.5, 1.0), transformationMatrix, pos);
    
    EndPrimitive();
}