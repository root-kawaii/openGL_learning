#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIDs;
layout (location = 6) in vec4 weights;

// Per-instance attributes (mat4 occupies locations 7-10, terrainType at 11)
layout (location = 7)  in mat4  instanceModel;
layout (location = 11) in float instanceTerrainType;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
    vec3 LocalPos;
    mat3 TBN;
} vs_out;

flat out int iTerrainType;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 lightSpaceMatrix;
uniform vec4 clipPlane;

void main()
{
    vec4 worldPos = instanceModel * vec4(aPos, 1.0);
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
    mat3 normalMatrix = transpose(inverse(mat3(instanceModel)));

    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = normalMatrix * aNormal;
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * worldPos;
    vs_out.LocalPos = aPos;

    // TBN matrix for normal mapping (world-space)
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    iTerrainType = int(round(instanceTerrainType));

    gl_Position = projection * view * worldPos;
}
