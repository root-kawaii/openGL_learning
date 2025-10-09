#version 330 core
out vec4 FragColor;

uniform vec3 baseColor;  // Base color input
uniform float time;      // Time input for animation

void main()
{
    // Create a pulsing glow effect using sine wave
    float glow = sin(time * 4.5) * 0.5 + 0.5; // Oscillates between 0 and 1
    
    // Amplify the glow effect
    float intensity = 0.5 + glow * 0.5; // Range from 0.5 to 2.0
    
    // Apply glow to the base color
    vec3 glowColor = baseColor * intensity;
    
    FragColor = vec4(glowColor, 1.0);
}