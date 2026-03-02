#version 330 core

// Per-vertex: local quad coords
// x: -0.5 to 0.5 (horizontal width along camera right)
// y:  0.0 to 1.0 (vertical position along fall direction)
layout(location = 0) in vec2 aLocalXY;

// Per-instance: world-space position
layout(location = 2) in vec3 instancePos;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform float streakWidth;
uniform float streakLength;

out float vLocalY;

void main()
{
    vec3 worldPos = instancePos
                  + cameraRight      * (aLocalXY.x * streakWidth)
                  + vec3(0,-1, 0)    * (aLocalXY.y * streakLength);

    vLocalY = aLocalXY.y;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
