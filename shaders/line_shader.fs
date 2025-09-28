#version 330 core
in vec2 texCoord;
out vec4 FragColor;

uniform vec3 color;
uniform float alpha;
uniform bool antiAlias;

void main()
{
    if (antiAlias) {
        // Calculate distance from center line (0.5 is center)
        float dist = abs(texCoord.y - 0.5) * 2.0; // 0 at center, 1 at edges
        
        // Create smooth falloff at edges
        float edgeSmooth = 1.0 - smoothstep(0.8, 1.0, dist);
        
        FragColor = vec4(color, alpha * edgeSmooth);
    } else {
        FragColor = vec4(color, alpha);
    }
}