#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D debugTexture;
uniform int visualizationMode; // 0=albedo, 1=normal, 2=position, 3=depth
uniform float depthNear;
uniform float depthFar;

// Function to visualize depth
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * depthNear * depthFar) / (depthFar + depthNear - z * (depthFar - depthNear));
}

void main()
{
    vec4 texColor = texture(debugTexture, TexCoord);
    
    if (visualizationMode == 0) {
        // Albedo - show as-is
        FragColor = vec4(texColor.rgb, 1.0);
    }
    else if (visualizationMode == 1) {
        // Normals - convert from [-1,1] to [0,1] for visualization
        FragColor = vec4(texColor.rgb * 0.5 + 0.5, 1.0);
    }
    else if (visualizationMode == 2) {
        // Position - normalize to reasonable range for visualization
        FragColor = vec4(abs(texColor.rgb) * 0.1, 1.0); // Adjust multiplier as needed
    }
    else if (visualizationMode == 3) {
        // Depth - linearize and visualize
        float depth = texColor.r; // already linearized
        FragColor = vec4(vec3(depth), 1.0);
    }
    else {
        // Default - show raw texture
        FragColor = texColor;
    }
}



// #version 330 core
// out vec4 FragColor;
// in vec2 TexCoord;

// uniform sampler2D debugTexture; 
// uniform float debugNearPlane; // Pass the camera's near plane (e.g., 0.1)
// uniform float debugFarPlane;  // Pass the camera's far plane (e.g., 100.0)

// void main() {
//     // Get the raw linear depth value from the red channel
//     float linearDepth = texture(debugTexture, TexCoord).r;

//     // Normalize the linear depth from [debugNearPlane, debugFarPlane] to [0, 1]
//     // This maps nearPlane to 0.0 (black) and farPlane to 1.0 (white)
//     float normalizedDepth = (linearDepth - debugNearPlane) / (debugFarPlane - debugNearPlane);

//     // Clamp the value to ensure it stays within [0,1] range for display
//     normalizedDepth = clamp(normalizedDepth, 0.0, 1.0);

//     // Output the normalized depth to all RGB channels for a grayscale visualization
//     FragColor = vec4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0);
// }