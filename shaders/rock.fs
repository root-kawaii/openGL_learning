#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    flat ivec4 boneIDs;
    vec4 weights;
} vs_out;

uniform vec3 objectColor;
uniform vec3 viewPos;
uniform vec3 lightColor;

// Toon quantise
float toon(float v, int bands) {
    return floor(v * float(bands)) / float(bands);
}

void main()
{
    vec3 N = normalize(vs_out.Normal);

    // Sun direction — matches sunset in cubemap.fs
    vec3 lightDir = normalize(vec3(0.55, 0.08, 0.40));

    float diff = max(dot(N, lightDir), 0.0);
    float cell = toon(diff, 3);

    // Warm sunset tint on lit side, cool purple on shadow side
    vec3 ambient  = 0.28 * objectColor;
    vec3 diffuse  = cell * vec3(1.0, 0.80, 0.55) * objectColor;

    // Subtle purple rim from the twilight side
    vec3  viewDir = normalize(viewPos - vs_out.FragPos);
    float rim     = pow(1.0 - max(dot(N, viewDir), 0.0), 3.0);
    vec3  rimCol  = rim * vec3(0.25, 0.05, 0.45) * 0.35;

    FragColor = vec4(ambient + diffuse + rimCol, 1.0);
}
