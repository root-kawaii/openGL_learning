#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform float lineWidth;
uniform vec2 screenSize;

out vec2 texCoord;

void main()
{
    // gl_in[0].gl_Position contains the world position from vertex shader
    vec4 worldPos = gl_in[0].gl_Position;
    
    // Apply full transformation to get clip space position
    vec4 clipPos = projection * view * model * worldPos;
    
    // Calculate line width in screen pixels
    vec2 ndcPerPixel = 2.0 / screenSize;
    float halfWidthNDC = lineWidth * 0.5 * ndcPerPixel.y; // Use Y for consistent aspect
    
    // Generate quad vertices around the point
    // Bottom-left
    gl_Position = clipPos + vec4(-halfWidthNDC, -halfWidthNDC, 0.0, 0.0);
    texCoord = vec2(0.0, 0.0);
    EmitVertex();
    
    // Bottom-right
    gl_Position = clipPos + vec4(halfWidthNDC, -halfWidthNDC, 0.0, 0.0);
    texCoord = vec2(1.0, 0.0);
    EmitVertex();
    
    // Top-left
    gl_Position = clipPos + vec4(-halfWidthNDC, halfWidthNDC, 0.0, 0.0);
    texCoord = vec2(0.0, 1.0);
    EmitVertex();
    
    // Top-right
    gl_Position = clipPos + vec4(halfWidthNDC, halfWidthNDC, 0.0, 0.0);
    texCoord = vec2(1.0, 1.0);
    EmitVertex();
    
    EndPrimitive();
}