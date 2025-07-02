#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
// REMOVED: in mat4 viewMatrix; // Matrices should be uniforms, not 'in' from VS

// G-buffer textures (gDepth is replaced by gLinearDepth for the main logic)
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gLinearDepth; // <-- Now directly sampling the linear depth

// Camera and Projection Matrices (from CPU)
uniform mat4 viewMatrix;          // View matrix (for view-space transformations)
uniform mat4 projection;          // Projection matrix
uniform mat4 viewProjection;      // projection * view
uniform mat4 inverseProjection;   // inverse(projection)
uniform mat4 inverseView;         // inverse(viewMatrix)
uniform mat4 inverseViewProjection; // inverse(viewProjection)

uniform vec3 cameraWorldPos;
uniform vec2 screenSize;
uniform float time;

// Camera parameters for linear depth calculation (matching G-buffer's)
uniform float nearPlane; // From C++, must match G-buffer's near_plane
uniform float farPlane;  // From C++, must match G-buffer's far_plane

const vec3 waterColor = vec3(0.1, 0.3, 0.6);
const float waterAlpha = 0.7;

// This LinearizeDepth is still needed if you want to use the raw gl_FragCoord.z
// from gDepth, but we'll try to avoid it by using gLinearDepth directly.
// However, the ReconstructWorldPosition still needs the original near/far for reconstruction!
// You need to pass the same near/far from the G-buffer shader to this water shader.
// The previous LinearizeDepth was for converting raw depth (0-1, non-linear) to linear view-space depth.
// If gLinearDepth already contains linear depth, you don't need to LinearizeDepth it again.
// But we still need the linear depth if we want to reconstruct.
// Let's assume the gLinearDepth texture already contains the view-space linear depth.

// The ReconstructWorldPosition function needs the inverse projection and inverse view.
// It also needs the 'raw' depth (0-1) from the depth buffer to map to clip space Z.
// Let's adjust ReconstructWorldPosition to take linear depth, and internally convert back if needed,
// OR more robustly, let's have it work with the linear depth directly and project from view space.

// Reconstruct World Position from Linear View-Space Depth
// This version takes linear view-space Z from the G-buffer and reconstructs world position.
// This is the core of getting correct reflections.
vec3 ReconstructWorldPosition(vec2 uv, float linearViewDepthSample, mat4 invProjection, mat4 invView) {
    // 1. Get clip-space position from UV and linear view depth
    // The linearViewDepthSample is already a view-space Z.
    // We need to 'unproject' it.
    
    // Convert UV to NDC [ -1, 1 ]
    vec2 ndc = uv * 2.0 - 1.0;

    // Create a point in view space with the known linear view depth
    // We need X and Y in view space based on NDC and projection matrix.
    // A more common approach is to project a point ON the near plane and ON the far plane
    // at the given UV, then interpolate between them to get the actual view space point.
    // However, since we have the linear depth, we can work directly.

    // 1. Get the direction vector from camera through this pixel in view space.
    // This involves unprojecting the NDC XY at Z=1 (far plane equivalent for direction).
    vec4 unprojectedFar = invProjection * vec4(ndc.x, ndc.y, 1.0, 1.0); // Z=1 is for direction
    vec3 viewRayDir = normalize(unprojectedFar.xyz / unprojectedFar.w);
    
    // Now, scale this direction by the actual linear depth from the G-buffer.
    // In view space, the camera is at (0,0,0) and looks down -Z. So linearViewDepthSample is the -Z value.
    vec3 viewSpacePos = viewRayDir * linearViewDepthSample; // Or -viewRayDir.z * linearViewDepthSample;

    // 2. Transform to World-Space
    return (invView * vec4(viewSpacePos, 1.0)).xyz;
}


// RaymarchReflection uses the more robust stepping approach
vec3 RaymarchReflection(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, mat4 invViewProj, mat4 invProj, mat4 invView, sampler2D linearDepthTex, sampler2D albedoTex) {
    vec3 reflDir = reflect(viewDir, normal);
    
    // Start the ray slightly above the surface to avoid self-intersection
    vec3 rayOrigin = worldPos + reflDir * 0.01; 

    const int steps = 64; // Increased steps for better quality
    const float maxRayDistance = 50.0; // Adjust this based on your scene's extent
    const float stepDistance = maxRayDistance / float(steps);
    
    // This tolerance is CRITICAL for hit detection.
    // It's in LINEAR VIEW SPACE Z. Tune this value!
    const float hitTolerance = 0.5; // Try values like 0.1, 0.5, 1.0, 2.0, 5.0

    for (int i = 0; i < steps; i++) {
        vec3 currentRayWorldPos = rayOrigin + reflDir * float(i) * stepDistance;

        // Project current ray point to screen space
        vec4 projCurrent = viewProj * vec4(currentRayWorldPos, 1.0);
        
        // Handle points behind camera or at/beyond far clipping plane
        if (projCurrent.w <= 0.0001) { // Small epsilon to avoid division by zero or artifacts
            break; 
        }
        
        projCurrent /= projCurrent.w; // Perspective divide to get NDC
        vec2 uv = projCurrent.xy * 0.5 + 0.5; // Convert NDC to UV [0,1]
        
        // Check if current UV is outside screen bounds
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            break;
        }
            
        // Get the linear view-space depth from the gLinearDepth texture
        float sceneLinearViewDepthSample = texture(linearDepthTex, uv).r;
        
        // Reconstruct the actual world position from the depth buffer at this UV
        // This is important if you need the actual sceneWorldPos for other calculations (e.g., normal)
        // However, for the hit test itself, we can use the linear view-space depths directly.
        // vec3 sceneWorldPos = ReconstructWorldPosition(uv, sceneLinearViewDepthSample, invProj, invView);

        // Get the linear view-space Z-depth of the current ray point
        vec4 rayViewPos = viewMatrix * vec4(currentRayWorldPos, 1.0);
        float rayLinearViewDepth = rayViewPos.z; // View-space Z is linear

        // --- HIT TEST ---
        // Condition 1: Ray point's depth (rayLinearViewDepth) is GREATER (further from camera)
        //              than the scene's actual depth at that UV (sceneLinearViewDepthSample).
        // AND
        // Condition 2: The difference between them is within a small tolerance.
        // This means the ray has passed "behind" the scene geometry at this pixel.
        if (rayLinearViewDepth > sceneLinearViewDepthSample && abs(rayLinearViewDepth - sceneLinearViewDepthSample) < hitTolerance) {
            // A hit! Return the albedo color from the scene at this UV.
            return texture(albedoTex, uv).rgb;
        }
    }
    return vec3(0.0); // Return black if no hit found after raymarching
}


void main() {
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    
    // Get animated water normal
    vec3 waterNormal = normalize(Normal);
    
    // Add wave animation
    vec2 waveOffset = sin(FragPos.xz * 3.0 + time * 2.0) * 0.02;
    waterNormal.x += waveOffset.x;
    waterNormal.z += waveOffset.y;
    waterNormal = normalize(waterNormal);
    
    // Get reflection color
    vec3 viewDir = normalize(cameraWorldPos - FragPos);

    // Call RaymarchReflection with the correct uniforms
    vec3 reflectionColor = RaymarchReflection(
        FragPos, viewDir, waterNormal, 
        viewProjection, inverseViewProjection, inverseProjection, inverseView, // Pass all necessary matrices
        gLinearDepth, gAlbedoSpec // Pass gLinearDepth and gAlbedoSpec samplers
    );    
    
    // Get refraction (scene behind water with slight distortion)
    // For more accurate refraction, you'd calculate the refracted ray and sample along it
    vec2 distortedUV = screenUV + waterNormal.xz * 0.03;
    distortedUV = clamp(distortedUV, 0.0, 1.0);
    vec3 refractionColor = texture(gAlbedoSpec, distortedUV).rgb;
    
    // Calculate fresnel
    float fresnel = pow(1.0 - max(dot(viewDir, waterNormal), 0.0), 3.0);
    
    // Mix colors
    // More physically intuitive mixing: mix refraction and reflection, then apply water color
    vec3 mixedReflRefr = mix(refractionColor, reflectionColor, fresnel);
    vec3 finalColor = mix(mixedReflRefr, waterColor, 0.4); // This 0.4 could be water's inherent transparency
    
    // Add some sparkle/shimmer
    float sparkle = sin(FragPos.x * 10.0 + time) * sin(FragPos.z * 10.0 + time) * 0.1 + 0.9;
    finalColor *= sparkle;
    
    FragColor = vec4(texture(gLinearDepth, screenUV).rrr, 1.0);
}