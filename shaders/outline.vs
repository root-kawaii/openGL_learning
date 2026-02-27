#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIDs;
layout (location = 6) in vec4 weights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform float outlineWidth;

void main()
{
    // Expand vertex along its normal in clip space.
    // Multiplying by clipPos.w keeps the outline a constant screen-space
    // thickness regardless of depth (avoids thin far outlines / thick near ones).
    vec4 clipPos = projection * view * model * vec4(aPos, 1.0);

    // Transform normal to clip space (ignore translation, only rotation+scale)
    mat3 normalMat = transpose(inverse(mat3(model)));
    vec3 worldNormal = normalize(normalMat * aNormal);
    vec4 clipNormal = projection * view * vec4(worldNormal, 0.0);

    // Push outward in clip XY by the outline width, scaled by w so the
    // screen-space offset is constant.
    clipPos.xy += normalize(clipNormal.xy) * outlineWidth * clipPos.w;

    gl_Position = clipPos;
}
