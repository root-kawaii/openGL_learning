#version 330 core
out vec4 FragColor;
in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} fs_in;

struct Light {
    vec3 Position;
    vec3 Color;
};

#define MAX_LIGHTS 8
uniform Light lights[MAX_LIGHTS];
uniform int numLights;
uniform sampler2D shadowMap;
uniform sampler2D texture_diffuse1;
uniform vec3 viewPos;

uniform int debugMode;

vec3 getBoneColor(int boneID) {
    float r = fract(sin(float(boneID) * 12.9898) * 43758.5453);
    float g = fract(sin(float(boneID) * 78.233) * 43758.5453);
    float b = fract(sin(float(boneID) * 45.164) * 43758.5453);
    return vec3(r, g, b);
}

const int levels = 3;
const float rimPower = 3.0;
const float rimThreshold = 0.5;
const float specularThreshold = 0.8;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 lightPos)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float bias = max(0.002 * (1.0 - dot(normal, lightDir)), 0.0002);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if(projCoords.z > 1.0) shadow = 0.0;
    return shadow;
}

void main()
{
    // Bone debug modes
    if (debugMode == 1) {
        vec3 boneColor = vec3(0.0);
        for (int i = 0; i < 4; i++) {
            if (fs_in.weights[i] > 0.0) {
                boneColor += getBoneColor(fs_in.boneIDs[i]) * fs_in.weights[i];
            }
        }
        float totalWeight = fs_in.weights.x + fs_in.weights.y + fs_in.weights.z + fs_in.weights.w;
        if (totalWeight < 0.01) boneColor = vec3(0.5);
        FragColor = vec4(boneColor, 1.0);
        return;
    }

    // Sample diffuse texture
    vec3 texColor = texture(texture_diffuse1, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    vec3 totalLighting = vec3(0.0);

    vec3 ambient = 0.15 * texColor;

    for(int i = 0; i < numLights; i++)
    {
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);

        float diff = max(dot(lightDir, normal), 0.0);
        float cellDiff = floor(diff * levels) / levels;
        float diffSmooth = smoothstep(0.0, 0.1, diff - floor(diff * levels) / levels);
        cellDiff = mix(cellDiff, cellDiff + 1.0/levels, diffSmooth * 0.3);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
        float cellSpec = smoothstep(specularThreshold - 0.05, specularThreshold + 0.05, spec);

        float shadow = 0.0;
        if(i == 0) {
            shadow = ShadowCalculation(fs_in.FragPosLightSpace, lights[i].Position);
        }
        float cellShadow = smoothstep(0.4, 0.6, shadow);

        vec3 diffuse = cellDiff * lights[i].Color * texColor;
        vec3 specular = cellSpec * lights[i].Color * 0.6;

        totalLighting += mix(diffuse + specular, vec3(0.0), cellShadow);
    }

    float rimDot = 1.0 - max(dot(viewDir, normal), 0.0);
    float rimIntensity = pow(rimDot, rimPower);
    rimIntensity = smoothstep(rimThreshold, 1.0, rimIntensity);
    vec3 rimColor = vec3(0.4, 0.7, 0.9) * rimIntensity * 0.6;

    vec3 finalColor = ambient + totalLighting + rimColor;

    float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
    finalColor = mix(vec3(luminance), finalColor, 1.15);

    FragColor = vec4(finalColor, 1.0);
}
