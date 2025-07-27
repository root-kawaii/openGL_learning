#version 330 core
layout (location = 0) out vec3 msaaGPosition;
layout (location = 1) out vec3 msaaGNormal;
layout (location = 2) out vec4 msaaGAlbedoSpec;
layout (location = 3) out float msaaGLinearDepth;  // Output linear depth
layout (location = 4) out vec3 msaaGMetallic; 
layout (location = 5) out vec3 msaaGRoughness; 

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in float viewDepth;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_metallic;
uniform sampler2D texture_roughness;


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
    msaaGPosition = FragPos;
    
    // Store the per-fragment normals into the gbuffer
    msaaGNormal = normalize(Normal);
    
    // Store the diffuse per-fragment color
    msaaGAlbedoSpec.rgb = texture(texture_diffuse, TexCoords).rgb;
    
    // Store specular intensity in gAlbedoSpec's alpha component
    msaaGAlbedoSpec.a = texture(texture_specular, TexCoords).r;
    
    // Calculate and store linear depth
    // Method 1: From gl_FragCoord.z
    msaaGLinearDepth = viewDepth;
    msaaGLinearDepth = (viewDepth - near_plane) / (far_plane - near_plane);

    msaaGMetallic = texture(texture_metallic, TexCoords).rgb;
    msaaGRoughness = texture(texture_roughness, TexCoords).rgb;
    
    // Method 2: Alternative - from distance to camera (try this if Method 1 doesn't work)
    // vec3 viewPos = vec3(0.0, 0.0, 0.0); // Camera position in view space
    // gLinearDepth = length(FragPos - viewPos); // Distance from camera
    
    // Method 3: Simple normalized depth (for testing)
    // gLinearDepth = (gl_FragCoord.z - near_plane) / (far_plane - near_plane);
}