#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

uniform sampler2D groundTexture;
uniform sampler2D shadowMap;

uniform vec3  viewPos;
uniform vec3  lightPos;
uniform vec3  lightColor;
uniform float texTiling;    // world-units per texture repeat (default 4.0)

// ---------------------------------------------------------------------------
// Shadow (PCF 3x3)
// ---------------------------------------------------------------------------
float ShadowCalculation(vec4 fragPosLS)
{
    vec3 proj  = fragPosLS.xyz / fragPosLS.w;
    proj       = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float currentDepth = proj.z;
    float bias         = 0.005;

    float shadow     = 0.0;
    vec2  texelSize  = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float pcf = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow   += currentDepth - bias > pcf ? 1.0 : 0.0;
        }
    shadow /= 9.0;
    return shadow;
}

// ---------------------------------------------------------------------------
// Toon quantise helper
// ---------------------------------------------------------------------------
float toonStep(float v, int bands)
{
    return floor(v * float(bands)) / float(bands);
}

void main()
{
    // Tiled UV from world position (avoids stretching on large plane)
    vec2 tiledUV = FragPos.xz / texTiling;
    vec3 albedo  = texture(groundTexture, tiledUV).rgb;

    vec3 normal   = vec3(0.0, 1.0, 0.0);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir  = normalize(viewPos - FragPos);

    // Toon diffuse (3 bands)
    float diff     = max(dot(normal, lightDir), 0.0);
    float cellDiff = toonStep(diff, 3);

    // Soft shadow into lighting
    float shadow = ShadowCalculation(FragPosLightSpace);
    float lit    = mix(cellDiff, 0.0, shadow * 0.6);

    vec3 ambient  = 0.20 * albedo;
    vec3 diffuse  = lit  * lightColor * albedo;

    // Subtle specular highlight
    vec3  halfway  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(normal, halfway), 0.0), 32.0);
    float cellSpec = smoothstep(0.75, 0.80, spec) * (1.0 - shadow);
    vec3  specular = cellSpec * lightColor * 0.15;

    vec3 finalColor = ambient + diffuse + specular;

    FragColor = vec4(finalColor, 1.0);
}
