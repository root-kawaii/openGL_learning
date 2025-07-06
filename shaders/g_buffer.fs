#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out float gLinearDepth;  // Output linear depth

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in float viewDepth;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;

// Camera parameters for linear depth calculation
uniform float near_plane;
uniform float far_plane;

// Convert view-space depth to linear depth
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main()
{    
    // Store the fragment position vector in the first gbuffer texture
    gPosition = FragPos;
    
    // Store the per-fragment normals into the gbuffer
    gNormal = normalize(Normal);
    
    // Store the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
    
    // Store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = texture(texture_specular1, TexCoords).r;
    
    // Calculate and store linear depth
    // Method 1: From gl_FragCoord.z
    gLinearDepth = viewDepth;
    gLinearDepth = (viewDepth - near_plane) / (far_plane - near_plane);
    
    // Method 2: Alternative - from distance to camera (try this if Method 1 doesn't work)
    // vec3 viewPos = vec3(0.0, 0.0, 0.0); // Camera position in view space
    // gLinearDepth = length(FragPos - viewPos); // Distance from camera
    
    // Method 3: Simple normalized depth (for testing)
    // gLinearDepth = (gl_FragCoord.z - near_plane) / (far_plane - near_plane);
}