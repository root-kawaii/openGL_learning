#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

uniform sampler2D shadowMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 objectColor;

// Cell shading parameters
const int levels = 4; // Number of lighting bands
const float edgeThreshold = 0.1; // For rim lighting/edge detection

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
    vec3 color = objectColor.rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    
    // Diffuse lighting with cell shading quantization
    float diff = max(dot(lightDir, normal), 0.0);
    float cellDiff = floor(diff * levels) / levels; // Quantize to discrete bands
    
    // Specular with cell shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    float cellSpec = step(0.5, spec); // Binary specular highlight
    
    // Rim lighting for cel-shaded edge glow
    float rimDot = 1.0 - max(dot(viewDir, normal), 0.0);
    float rimIntensity = smoothstep(0.6, 1.0, rimDot);
    vec3 rimColor = vec3(0.2, 0.2, 0.3) * rimIntensity;
    
    // Calculate shadow
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);
    
    // Cell-shaded shadow (binary)
    float cellShadow = step(0.5, shadow);
    
    // Combine lighting
    vec3 ambient = 0.3 * color;
    vec3 diffuse = cellDiff * 0.7 * color;
    vec3 specular = 0 * vec3(1.0, 1.0, 1.0) * 0.4;
    
    // Apply shadow (darkens non-ambient lighting)
    vec3 lighting = ambient + (1.0 - cellShadow) * (diffuse + specular) + rimColor;
    
    FragColor = vec4(lighting, 1.0);
}