#version 330 core
layout (location = 0) in float vertexIndex;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 startPos;
uniform vec3 targetPos;
uniform int segments;
uniform float arcHeightMultiplier;
uniform float pointSize;
uniform float arcFactor; // 1.0 = parabolic, 0.0 = linear

void main()
{
    // Calculate trajectory position
    float t = vertexIndex / float(segments);
    vec3 displacement = targetPos - startPos;
    float distance = length(displacement.xz);
    float heightDiff = displacement.y;

    float x = startPos.x + t * displacement.x;
    float z = startPos.z + t * displacement.z;

    // Parabolic arc height (only applied when arcFactor > 0)
    float arcHeight = max(distance * arcHeightMultiplier, abs(heightDiff) + 2.0);
    float parabolicOffset = 4.0 * arcHeight * t * (1.0 - t) * arcFactor;
    float y = startPos.y + t * heightDiff + parabolicOffset;

    vec3 worldPos = vec3(x, y, z);

    // Apply model matrix here, geometry shader expects world positions
    gl_Position = model * vec4(worldPos, 1.0);
}
