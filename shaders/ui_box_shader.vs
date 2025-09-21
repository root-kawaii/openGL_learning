#version 330 core
layout (location = 0) in vec3 aPos;        // Vertex position (-1 to 1 range)
layout (location = 1) in vec2 aTexCoords;  // Texture coordinates

out vec2 TexCoords;

uniform vec2 screenSize;    // Screen dimensions (width, height)
uniform vec2 position;      // UI element position in pixels
uniform vec2 size;          // UI element size in pixels

void main()
{
    TexCoords = aTexCoords;
    
    // Scale the unit quad by the desired size in pixels
    vec2 scaledPos = aPos.xy * size;
    
    // Translate to the desired position (add offset)
    vec2 worldPos = scaledPos + position + (size * 0.5); // Center the quad
    
    // Convert screen coordinates to NDC space
    vec2 ndcPos;
    ndcPos.x = (worldPos.x / screenSize.x) * 2.0 - 1.0;  // X: 0->width becomes -1->1
    ndcPos.y = 1.0 - (worldPos.y / screenSize.y) * 2.0;  // Y: 0->height becomes 1->-1 (flip Y)
    
    // Output final position directly in clip space
    gl_Position = vec4(ndcPos, 0.0, 1.0);
}