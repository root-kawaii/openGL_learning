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

// Define a Light structure
struct Light {
    vec3 Position;
    vec3 Color;
};

#define MAX_LIGHTS 8
uniform Light lights[MAX_LIGHTS];
uniform int numLights; // How many lights are actually active
uniform sampler2D shadowMap;
uniform vec3 viewPos;
uniform vec3 objectColor;

// Debug mode: 0 = normal, 1 = bone visualization, 2 = weight heatmap, 3 = dominant bone
uniform int debugMode;

// Generate distinct color for each bone ID
vec3 getBoneColor(int boneID) {
    // Use hash-like function to generate distinct colors
    float r = fract(sin(float(boneID) * 12.9898) * 43758.5453);
    float g = fract(sin(float(boneID) * 78.233) * 43758.5453);
    float b = fract(sin(float(boneID) * 45.164) * 43758.5453);
    return vec3(r, g, b);
}

const float BANDS  = 4.0;   // hard light quantization steps
const float POSTER = 6.0;   // posterization levels per channel

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
    // Bone visualization mode
    if (debugMode == 1) {
        // Visualize bone influences
        vec3 boneColor = vec3(0.0);

        // Mix colors based on bone weights
        for (int i = 0; i < 4; i++) {
            if (fs_in.weights[i] > 0.0) {
                vec3 influenceColor = getBoneColor(fs_in.boneIDs[i]);
                boneColor += influenceColor * fs_in.weights[i];
            }
        }

        // If no bones influence this vertex, show it in gray
        float totalWeight = fs_in.weights.x + fs_in.weights.y + fs_in.weights.z + fs_in.weights.w;
        if (totalWeight < 0.01) {
            boneColor = vec3(0.5); // Gray for non-rigged vertices
        }

        FragColor = vec4(boneColor, 1.0);
        return;
    }

    // Bone weight heatmap mode
    if (debugMode == 2) {
        // Show weight distribution as heatmap
        float totalWeight = fs_in.weights.x + fs_in.weights.y + fs_in.weights.z + fs_in.weights.w;

        // Heatmap: blue (0) -> green (0.5) -> red (1)
        vec3 heatmap;
        if (totalWeight < 0.5) {
            heatmap = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), totalWeight * 2.0);
        } else {
            heatmap = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (totalWeight - 0.5) * 2.0);
        }

        FragColor = vec4(heatmap, 1.0);
        return;
    }

    // Dominant bone visualization mode
    if (debugMode == 3) {
        // Show only the most influential bone
        int dominantBone = fs_in.boneIDs[0];
        float maxWeight = fs_in.weights[0];

        for (int i = 1; i < 4; i++) {
            if (fs_in.weights[i] > maxWeight) {
                maxWeight = fs_in.weights[i];
                dominantBone = fs_in.boneIDs[i];
            }
        }

        vec3 dominantColor = getBoneColor(dominantBone);
        if (maxWeight < 0.01) {
            dominantColor = vec3(0.5); // Gray for non-rigged
        }

        FragColor = vec4(dominantColor, 1.0);
        return;
    }

    // Normal rendering mode (debugMode == 0)
    vec3 color  = objectColor.rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 totalDiffuse = vec3(0.0);

    for(int i = 0; i < numLights; i++)
    {
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
        float diff    = max(dot(lightDir, normal), 0.0);
        float band    = floor(diff * BANDS) / BANDS;   // hard step, no smoothstep
        totalDiffuse += band * lights[i].Color;
    }

    // Hard binary shadow
    float shadow   = (numLights > 0) ? ShadowCalculation(fs_in.FragPosLightSpace, lights[0].Position) : 0.0;
    float inShadow = step(0.5, shadow);

    vec3 ambient    = 0.20 * color;
    vec3 lit        = totalDiffuse * color;
    vec3 finalColor = ambient + lit * (1.0 - inShadow * 0.7);

    // Posterize
    finalColor = floor(finalColor * POSTER + 0.5) / POSTER;

    FragColor = vec4(finalColor, 1.0);
}
