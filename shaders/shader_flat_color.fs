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

uniform sampler2D shadowMap;
uniform vec3 lightPos;
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

// Enhanced cell shading parameters
const int levels = 3; // Fewer bands for stronger toon effect
const float rimPower = 3.0; // Sharper rim falloff
const float rimThreshold = 0.5; // Rim light threshold
const float specularThreshold = 0.8; // Tighter specular highlights

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    if(projCoords.z > 1.0)
        shadow = 0.0;
    
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
    vec3 color = objectColor.rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    
    // Enhanced diffuse with sharper bands
    float diff = max(dot(lightDir, normal), 0.0);
    float cellDiff = floor(diff * levels) / levels;
    
    // Add half-tone transition for smoother bands
    float diffSmooth = smoothstep(0.0, 0.1, diff - floor(diff * levels) / levels);
    cellDiff = mix(cellDiff, cellDiff + 1.0/levels, diffSmooth * 0.3);
    
    // Enhanced specular with tighter highlights
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    float cellSpec = smoothstep(specularThreshold - 0.05, specularThreshold + 0.05, spec);
    
    // Enhanced rim lighting with color tint
    float rimDot = 1.0 - max(dot(viewDir, normal), 0.0);
    float rimIntensity = pow(rimDot, rimPower);
    rimIntensity = smoothstep(rimThreshold, 1.0, rimIntensity);
    
    // Colored rim light (cyan/blue tint for tech aesthetic)
    vec3 rimColor = vec3(0.4, 0.7, 0.9) * rimIntensity * 0.6;
    
    // Calculate shadow with cel-shaded transition
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);
    float cellShadow = smoothstep(0.4, 0.6, shadow);
    
    // Enhanced lighting with better contrast
    vec3 ambient = 0.25 * color;
    vec3 diffuse = cellDiff * 0.8 * color;
    vec3 specular = cellSpec * vec3(1.0, 1.0, 0.95) * 0.6;
    
    // Apply shadow with more dramatic darkening
    vec3 shadowColor = color * 0.15; // Dark shadow color
    vec3 litColor = diffuse + specular;
    
    // Combine with enhanced contrast
    vec3 lighting = ambient + mix(litColor, shadowColor, cellShadow) + rimColor;
    
    // Optional: Add slight saturation boost
    float luminance = dot(lighting, vec3(0.299, 0.587, 0.114));
    lighting = mix(vec3(luminance), lighting, 1.15);
    
    FragColor = vec4(lighting, 1.0);
}
