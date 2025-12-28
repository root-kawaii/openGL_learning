#version 330 core
out vec4 FragColor;

// Input from vertex shader
in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} vs_out;

// Texture sampler
uniform sampler2D texture_diffuse1;

// Optional: Basic lighting
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// Debug mode: 0 = normal, 1 = bone visualization
uniform int debugMode;

// Generate distinct color for each bone ID
vec3 getBoneColor(int boneID) {
    // Use hash-like function to generate distinct colors
    float r = fract(sin(float(boneID) * 12.9898) * 43758.5453);
    float g = fract(sin(float(boneID) * 78.233) * 43758.5453);
    float b = fract(sin(float(boneID) * 45.164) * 43758.5453);
    return vec3(r, g, b);
}

void main()
{
    // Sample the texture
    vec3 color = texture(texture_diffuse1, vs_out.TexCoords).rgb;

    // Bone visualization mode
    if (debugMode == 1) {
        // Visualize bone influences
        vec3 boneColor = vec3(0.0);

        // Mix colors based on bone weights
        for (int i = 0; i < 4; i++) {
            if (vs_out.weights[i] > 0.0) {
                vec3 influenceColor = getBoneColor(vs_out.boneIDs[i]);
                boneColor += influenceColor * vs_out.weights[i];
            }
        }

        // If no bones influence this vertex, show it in gray
        float totalWeight = vs_out.weights.x + vs_out.weights.y + vs_out.weights.z + vs_out.weights.w;
        if (totalWeight < 0.01) {
            boneColor = vec3(0.5); // Gray for non-rigged vertices
        }

        FragColor = vec4(boneColor, 1.0);
        return;
    }

    // Bone weight heatmap mode
    if (debugMode == 2) {
        // Show weight distribution as heatmap
        float totalWeight = vs_out.weights.x + vs_out.weights.y + vs_out.weights.z + vs_out.weights.w;

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
        int dominantBone = vs_out.boneIDs[0];
        float maxWeight = vs_out.weights[0];

        for (int i = 1; i < 4; i++) {
            if (vs_out.weights[i] > maxWeight) {
                maxWeight = vs_out.weights[i];
                dominantBone = vs_out.boneIDs[i];
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
    vec3 ambient = 0.99 * color;

    // Diffuse lighting
    vec3 lightColor = vec3(1.0);
    vec3 lightDir = normalize(lightPos - vs_out.FragPos);
    vec3 normal = normalize(vs_out.Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    // Simple specular
    vec3 viewDir = normalize(viewPos - vs_out.FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    // Combine lighting
    vec3 result = (ambient + diffuse + specular) * color;

    FragColor = vec4(result, 1.0);
}