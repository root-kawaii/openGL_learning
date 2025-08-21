#version 330 core

uniform vec3 axisColor = vec3(0.3, 0.3, 0.3); // Default gray color
out vec4 FragColor;

void main()
{
    FragColor = vec4(axisColor, 1.0);
}