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

const float BANDS  = 4.0;
const float POSTER = 6.0;

void main()
{
    vec3 N = normalize(vs_out.Normal);

    vec3 lightDir = normalize(vec3(0.55, 0.08, 0.40));
    float diff    = max(dot(N, lightDir), 0.0);
    float band    = floor(diff * BANDS) / BANDS;

    vec3 ambient    = 0.20 * objectColor;
    vec3 diffuse    = band * vec3(1.0, 0.80, 0.55) * objectColor;
    vec3 finalColor = ambient + diffuse;

    finalColor = floor(finalColor * POSTER + 0.5) / POSTER;

    FragColor = vec4(finalColor, 1.0);
}
