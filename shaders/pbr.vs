// =============================================================================
//  pbr.vs  —  Shared vertex shader for all PBR fragment shaders
//
//  Responsibilities:
//    1. Transform each vertex from object-space → clip-space
//    2. Apply skeletal bone transforms (same rig as shader.vs) so that
//       animated GLB/FBX models render correctly
//    3. Pass world-space position and normal to the fragment shader so that
//       lighting math can be done in a consistent coordinate system
//    4. Build the TBN matrix so the fragment shader can transform normal-map
//       vectors from tangent-space into world-space
//    5. Compute FragPosLightSpace for the shadow map comparison
// =============================================================================
#version 330 core

// ── Vertex attributes ─────────────────────────────────────────────────────────
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in ivec4 boneIDs;   // up to 4 bone influences per vertex
layout(location = 6) in vec4  weights;   // corresponding blend weights

// ── Outputs to the fragment shader ───────────────────────────────────────────
out VS_OUT {
    vec3 WorldPos;           // world-space fragment position
    vec3 Normal;             // world-space geometric normal
    vec2 TexCoords;
    vec4 FragPosLightSpace;  // position in light's clip-space (for shadow map)
    mat3 TBN;                // tangent → world transform (for normal maps)
} vs_out;

// ── Uniforms set by the C++ render pass ──────────────────────────────────────
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform vec4 clipPlane;      // for planar reflections (water); pass (0,-1,0,1e6) to disable

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

void main()
{
    // ── Skeletal animation ────────────────────────────────────────────────────
    // Mirror the bone blending from shader.vs so animated GLB models work.
    // If the mesh has no bones, totalWeight == 0 and we fall back to aPos/aNormal.
    float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];

    vec4 localPos;
    vec4 localNormal;

    if (totalWeight > 0.0)
    {
        mat4 BoneTransform  = gBones[boneIDs[0]] * weights[0];
        BoneTransform      += gBones[boneIDs[1]] * weights[1];
        BoneTransform      += gBones[boneIDs[2]] * weights[2];
        BoneTransform      += gBones[boneIDs[3]] * weights[3];

        localPos    = BoneTransform * vec4(aPos,    1.0);
        localNormal = BoneTransform * vec4(aNormal, 0.0);
    }
    else
    {
        localPos    = vec4(aPos,    1.0);
        localNormal = vec4(aNormal, 0.0);
    }

    vec4 worldPos = model * localPos;

    // ── Clip plane (used by water reflection pass) ────────────────────────────
    gl_ClipDistance[0] = dot(worldPos, clipPlane);

    // ── Normal matrix: inverse-transpose of model removes non-uniform scale ──
    // (Regular model matrix would stretch normals if the object is scaled.)
    mat3 normalMatrix = transpose(inverse(mat3(model)));

    vs_out.WorldPos          = worldPos.xyz;
    vs_out.Normal            = normalize(normalMatrix * localNormal.xyz);
    vs_out.TexCoords         = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * worldPos;

    // ── TBN — orthonormal tangent-space basis in world coordinates ────────────
    // We re-orthogonalise T against N (Gram-Schmidt) to handle numerical drift
    // from the normal matrix transformation.
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = vs_out.Normal;
    T = normalize(T - dot(T, N) * N);   // make T exactly perpendicular to N
    vec3 B = cross(N, T);

    vs_out.TBN = mat3(T, B, N);         // columns: tangent, bitangent, normal

    gl_Position = projection * view * worldPos;
}
