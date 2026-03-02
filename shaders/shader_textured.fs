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

const float BANDS  = 4.0;
const float POSTER = 6.0;

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
    vec3 normal   = normalize(fs_in.Normal);
    vec3 totalDiffuse = vec3(0.0);

    for(int i = 0; i < numLights; i++)
    {
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
        float diff    = max(dot(lightDir, normal), 0.0);
        float band    = floor(diff * BANDS) / BANDS;
        totalDiffuse += band * lights[i].Color;
    }

    float shadow   = (numLights > 0) ? ShadowCalculation(fs_in.FragPosLightSpace, lights[0].Position) : 0.0;
    float inShadow = step(0.5, shadow);

    vec3 ambient    = 0.20 * texColor;
    vec3 lit        = totalDiffuse * texColor;
    vec3 finalColor = ambient + lit * (1.0 - inShadow * 0.7);

    finalColor = floor(finalColor * POSTER + 0.5) / POSTER;

    FragColor = vec4(finalColor, 1.0);
}
