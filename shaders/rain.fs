#version 330 core

in float vLocalY;

uniform float opacity;

out vec4 FragColor;

void main()
{
    // Fade at the head (y=0) and tail (y=1) of the streak
    float fade = smoothstep(0.0, 0.15, vLocalY) * smoothstep(1.0, 0.8, vLocalY);
    float alpha = opacity * fade;

    // Cool blue-white tint that fits toon palette
    FragColor = vec4(0.75, 0.88, 1.0, alpha);
}
