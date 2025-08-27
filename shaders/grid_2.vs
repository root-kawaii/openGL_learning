#version 330 core

layout (location = 0) in vec3 aPos;        // Base geometry
layout (location = 1) in vec3 startPos;    // Instance: start position
layout (location = 2) in vec3 direction;   // Instance: line direction
layout (location = 3) in float thickness;  // Instance: line thickness
layout (location = 4) in float length;     // Instance: line length
layout (location = 5) in vec3 color;       // Instance: line color

uniform mat4 view;
uniform mat4 projection;

out vec3 fragColor;

void main() {
    // Create transformation matrix for this instance
    vec3 forward = normalize(direction);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(forward, up));
    up = normalize(cross(right, forward));
    
    // Create rotation matrix
    mat3 rotation = mat3(
        right * thickness,
        up * thickness, 
        forward * length
    );
    
    // Transform the base vertex
    vec3 worldPos = startPos + rotation * aPos;
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
    fragColor = color;
}