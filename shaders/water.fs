#version 330 core
out vec4 FragColor;

in vec2 TexCoords; // This would typically be gl_FragCoord.xy / screenSize for SSR

uniform sampler2D gPosition; // Now world space position
uniform sampler2D gNormal;   // Now world space normal
uniform sampler2D gAlbedoSpec; // For original scene color (can rename to sceneColor)

// Uniforms for reconstructing/projecting
uniform mat4 viewProjection;       // World -> Clip matrix (projection * view)
uniform mat4 inverseViewProjection; // Clip -> World matrix (inverse(viewProjection))
uniform vec3 cameraWorldPos;       // The camera's world position (replaces viewPos)
uniform vec2 screenSize;

// Constants (remain the same)
const int maxSteps = 64;
const float stepSize = 0.1;
const float thickness = 0.01;

// Function to reconstruct World-Space Position from screen UV and G-buffer depth
// OR, even simpler: just sample gPosition directly for scenePos.
vec3 ReconstructWorldPos(vec2 uv, float depth) // Keeping this name but note it gives world pos
{
    // This function can be complex if you truly want to reconstruct from a raw depth buffer.
    // However, since you have gPosition, it's often better to just sample gPosition.
    // Let's assume for this example, gDepth is camera-space depth (0-1) and you need to convert to world pos.
    // This would require inverse of your camera's projection *and* view matrix.
    // A simpler approach if gPosition is available:
    return texture(gPosition, uv).rgb; // Direct sample from gPosition
}

// RayMarch needs to operate in world space now
vec3 RayMarch(vec3 fragWorldPos, vec3 reflectWorldDir)
{
    float t = 0.0;

    for (int i = 0; i < maxSteps; ++i)
    {
        t += stepSize;
        vec3 sampleWorldPos = fragWorldPos + reflectWorldDir * t;

        // Project world position to screen space
        vec4 projected = viewProjection * vec4(sampleWorldPos, 1.0);
        projected /= projected.w;
        vec2 uv = projected.xy * 0.5 + 0.5;

        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
            break;

        // Get the actual world position from the G-buffer at the sampled UV
        vec3 sceneWorldPos = texture(gPosition, uv).rgb;

        // Check if the sampled world position is close to the ray-marched position
        // You might use distance here, or check along the Z axis relative to camera
        // For simplicity, let's keep the Z check, but adapt to world-space Z for consistency.
        // This check is the trickiest part as it depends on how you define 'depth' in world space.
        // A better check might be:
        if (distance(sceneWorldPos, sampleWorldPos) < thickness)
        {
            return texture(gAlbedoSpec, uv).rgb; // Sample albedo for reflection color
        }
        // Or if you only stored depth in gDepth (from camera view)
        // float sceneCameraSpaceDepth = texture(gDepth, uv).r;
        // vec3 reconstructedViewPos = ReconstructViewPosFromCameraDepth(uv, sceneCameraSpaceDepth, inverseProjection); // need a new function for this
        // if (abs(reconstructedViewPos.z - yourCurrentRayPointInViewSpace.z) < thickness) ...
        // Using gPosition for intersection is generally simpler.
    }

    return vec3(0.0);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / screenSize; // Get screen-space UV
    
    // Sample G-Buffer values for the current fragment
    vec3 fragWorldPos = texture(gPosition, uv).rgb;
    vec3 normalWorld = normalize(texture(gNormal, uv).rgb); // Assuming gNormal stores already normalized world normals
    vec3 albedo = texture(gAlbedoSpec, uv).rgb;

    // Early exit for background/sky pixels (if gPosition provides 'infinity' for sky)
    // Or if depth from gDepth (main camera depth) is at the far plane.
    // float depth = texture(gDepth, uv).r;
    // if (depth >= 0.999) { // Assuming gDepth is camera-space and 1.0 means sky
    //     FragColor = vec4(albedo, 1.0); // Use the scene color directly
    //     return;
    // }
    // More robust check: if gPosition is (0,0,0) or some sentinel for sky.
    // Or simply: if length(fragWorldPos) > veryLargeNumber (e.g., far_plane of G-Buffer)

    // Calculate view direction in world space
    vec3 viewWorldDir = normalize(cameraWorldPos - fragWorldPos);
    
    // Calculate reflection direction in world space
    vec3 reflectWorldDir = reflect(viewWorldDir, normalWorld);

    // Optional early-out optimization (still valid in world space)
    if (dot(viewWorldDir, normalWorld) > 0.99)
    {
        FragColor = vec4(albedo, 1.0); // Use original scene color
        return;
    }

    vec3 reflectedColor = RayMarch(fragWorldPos, reflectWorldDir);
    float fresnel = pow(1.0 - dot(viewWorldDir, normalWorld), 3.0);

    vec3 finalColor = mix(albedo, reflectedColor, fresnel);
    FragColor = vec4(finalColor, 1.0); // Alpha 1.0 for final display
}