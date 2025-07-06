#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec4 ClipSpacePos;

// G-buffer textures
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gLinearDepth;

// Camera and Projection Matrices
uniform mat4 viewMatrix;
uniform mat4 projection;
uniform mat4 viewProjection;
uniform mat4 inverseProjection;
uniform mat4 inverseView;
uniform mat4 inverseViewProjection;

uniform vec3 cameraWorldPos;
uniform vec2 screenSize;
uniform float time;

// Camera parameters
uniform float nearPlane;
uniform float farPlane;

const vec3 waterColor = vec3(0.1, 0.3, 0.6);
const float waterAlpha = 0.7;

// Convert normalized linear depth (0-1) back to view-space depth
float NormalizedToViewDepth(float normalizedDepth) {
    return normalizedDepth * (farPlane - nearPlane) + nearPlane;
}

// Balanced SSR with selective occlusion checking
vec3 BalancedSSR(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, sampler2D depthTex, sampler2D colorTex) {
    vec3 reflDir = reflect(-viewDir, normal);
    
    // Start ray slightly above surface to avoid self-intersection
    vec3 rayStart = worldPos + normal * 0.02;
    
    const int maxSteps = 75;
    const float maxDistance = 30.0;
    const float thickness = 1;
    
    // Transform to view space for consistent depth comparison
    vec4 viewRayStart = viewMatrix * vec4(rayStart, 1.0);
    vec4 viewReflDir = viewMatrix * vec4(reflDir, 0.0);
    
    vec3 rayStartView = viewRayStart.xyz;
    vec3 rayDirView = normalize(viewReflDir.xyz);
    
    float stepSize = maxDistance / float(maxSteps);
    
    for (int i = 1; i < maxSteps; i++) {
        vec3 currentViewPos = rayStartView + rayDirView * float(i) * stepSize;
        
        // Project to screen space
        vec4 projPos = projection * vec4(currentViewPos, 1.0);
        
        // Skip if behind camera
        if (projPos.w <= 0.0) break;
        
        // Convert to screen UV
        vec2 screenUV = (projPos.xy / projPos.w) * 0.5 + 0.5;
        
        // Check screen bounds
        if (screenUV.x < 0.0 || screenUV.x > 1.0 || 
            screenUV.y < 0.0 || screenUV.y > 1.0) {
            break;
        }
        
        // Sample scene depth
        float sceneDepthNorm = texture(depthTex, screenUV).r;
        
        // Skip background/sky
        if (sceneDepthNorm >= 0.999) continue;
        
        // Convert to view space depth
        float sceneDepth = NormalizedToViewDepth(sceneDepthNorm);
        float rayDepth = -currentViewPos.z; // View space Z is negative
        
        // Check for intersection with thickness tolerance
        if (rayDepth > sceneDepth && rayDepth - sceneDepth < thickness) {
            // SELECTIVE OCCLUSION CHECK: Only check for major occlusions
            bool occluded = false;
            
            // Only do occlusion checking for distant reflections (more likely to be wrong)
            if (i > maxSteps / 3) {
                int occlusionSamples = 30; // Minimal sampling
                
                for (int j = 1; j < occlusionSamples; j++) {
                    float t = float(j) / float(occlusionSamples);
                    vec3 sampleViewPos = rayStartView + rayDirView * float(i) * stepSize * t;
                    
                    vec4 sampleProjPos = projection * vec4(sampleViewPos, 1.0);
                    if (sampleProjPos.w <= 0.0) continue;
                    
                    vec2 sampleUV = (sampleProjPos.xy / sampleProjPos.w) * 0.5 + 0.5;
                    if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || 
                        sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;
                    
                    float sampleDepthNorm = texture(depthTex, sampleUV).r;
                    if (sampleDepthNorm >= 0.999) continue;
                    
                    float sampleSceneDepth = NormalizedToViewDepth(sampleDepthNorm);
                    float sampleRayDepth = -sampleViewPos.z;
                    
                    // More lenient occlusion test - only block if significantly behind
                    if (sampleRayDepth > sampleSceneDepth + thickness * 2.0) {
                        occluded = true;
                        break;
                    }
                }
            }
            
            if (!occluded) {
                // Calculate fade based on distance from screen center and ray length
                vec2 centerDist = abs(screenUV - 0.5);
                float edgeFade = 1.0 - smoothstep(0.35, 0.5, max(centerDist.x, centerDist.y));
                float distanceFade = 1.0 - smoothstep(0.5, 1.0, float(i) / float(maxSteps));
                
                vec3 reflColor = texture(colorTex, screenUV).rgb;
                return reflColor * edgeFade * distanceFade;
            }
        }
    }
    
    return vec3(0.0);
}

// Multiple ray SSR - cast several rays to work around occlusion
vec3 MultiRaySSR(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, sampler2D depthTex, sampler2D colorTex) {
    vec3 reflDir = reflect(-viewDir, normal);
    vec3 rayStart = worldPos + normal * 0.05;
    
    // Create basis vectors for ray perturbation
    vec3 tangent = normalize(cross(normal, vec3(0, 1, 0)));
    if (length(tangent) < 0.5) tangent = normalize(cross(normal, vec3(1, 0, 0)));
    vec3 bitangent = normalize(cross(normal, tangent));
    
    vec3 totalReflection = vec3(0.0);
    float totalWeight = 0.0;
    
    // Cast multiple rays with slight angular offsets
    for (int rayIndex = 0; rayIndex < 9; rayIndex++) {
        float angle = float(rayIndex) * 0.785398; // 45 degrees in radians
        float radius = (rayIndex == 0) ? 0.0 : 0.03; // Center ray has no offset
        
        vec2 offset = vec2(cos(angle), sin(angle)) * radius;
        vec3 perturbedReflDir = normalize(reflDir + tangent * offset.x + bitangent * offset.y);
        
        vec4 viewRayStart = viewMatrix * vec4(rayStart, 1.0);
        vec4 viewReflDir = viewMatrix * vec4(perturbedReflDir, 0.0);
        
        vec3 rayStartView = viewRayStart.xyz;
        vec3 rayDirView = normalize(viewReflDir.xyz);
        
        const int steps = 60;
        const float maxDist = 30.0;
        float stepSize = maxDist / float(steps);
        
        for (int i = 1; i < steps; i++) {
            vec3 currentViewPos = rayStartView + rayDirView * float(i) * stepSize;
            
            vec4 projPos = projection * vec4(currentViewPos, 1.0);
            if (projPos.w <= 0.0) break;
            
            vec2 screenUV = (projPos.xy / projPos.w) * 0.5 + 0.5;
            if (screenUV.x < 0.0 || screenUV.x > 1.0 || 
                screenUV.y < 0.0 || screenUV.y > 1.0) break;
            
            float sceneDepthNorm = texture(depthTex, screenUV).r;
            if (sceneDepthNorm >= 0.999) continue;
            
            float sceneDepth = NormalizedToViewDepth(sceneDepthNorm);
            float rayDepth = -currentViewPos.z;
            
            if (rayDepth > sceneDepth && rayDepth - sceneDepth < 0.5) {
                vec2 centerDist = abs(screenUV - 0.5);
                float edgeFade = 1.0 - smoothstep(0.35, 0.5, max(centerDist.x, centerDist.y));
                float distanceFade = 1.0 - smoothstep(0.5, 1.0, float(i) / float(steps));
                
                vec3 reflColor = texture(colorTex, screenUV).rgb;
                float weight = edgeFade * distanceFade;
                if (rayIndex == 0) weight *= 2.0; // Give more weight to center ray
                
                totalReflection += reflColor * weight;
                totalWeight += weight;
                break;
            }
        }
    }
    
    return totalWeight > 0.0 ? totalReflection / totalWeight : vec3(0.0);
}

// Alternative: Multi-sample SSR that tries different ray offsets
vec3 MultiSampleSSR(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, sampler2D depthTex, sampler2D colorTex) {
    vec3 reflDir = reflect(-viewDir, normal);
    vec3 rayStart = worldPos + normal * 0.02;
    
    // Try multiple slightly different ray directions to work around occlusion
    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = normalize(cross(normal, tangent));
    
    vec3 totalReflection = vec3(0.0);
    float totalWeight = 0.0;
    
    // Sample offsets - small perturbations to the reflection direction
    vec2 sampleOffsets[5] = vec2[](
        vec2(0.0, 0.0),     // Center
        vec2(0.05, 0.0),    // Right
        vec2(-0.05, 0.0),   // Left
        vec2(0.0, 0.05),    // Up
        vec2(0.0, -0.05)    // Down
    );
    
    float sampleWeights[5] = float[](
        0.4, 0.15, 0.15, 0.15, 0.15
    );
    
    for (int sample = 0; sample < 5; sample++) {
        vec2 offset = sampleOffsets[sample];
        vec3 perturbedReflDir = normalize(reflDir + tangent * offset.x + bitangent * offset.y);
        
        // Simplified raymarching for each sample
        vec4 viewRayStart = viewMatrix * vec4(rayStart, 1.0);
        vec4 viewReflDir = viewMatrix * vec4(perturbedReflDir, 0.0);
        
        vec3 rayStartView = viewRayStart.xyz;
        vec3 rayDirView = normalize(viewReflDir.xyz);
        
        const int steps = 50; // Fewer steps per sample
        const float maxDist = 25.0;
        float stepSize = maxDist / float(steps);
        
        for (int i = 1; i < steps; i++) {
            vec3 currentViewPos = rayStartView + rayDirView * float(i) * stepSize;
            
            vec4 projPos = projection * vec4(currentViewPos, 1.0);
            if (projPos.w <= 0.0) break;
            
            vec2 screenUV = (projPos.xy / projPos.w) * 0.5 + 0.5;
            if (screenUV.x < 0.0 || screenUV.x > 1.0 || 
                screenUV.y < 0.0 || screenUV.y > 1.0) break;
            
            float sceneDepthNorm = texture(depthTex, screenUV).r;
            if (sceneDepthNorm >= 0.999) continue;
            
            float sceneDepth = NormalizedToViewDepth(sceneDepthNorm);
            float rayDepth = -currentViewPos.z;
            
            if (rayDepth > sceneDepth && rayDepth - sceneDepth < 0.4) {
                vec2 centerDist = abs(screenUV - 0.5);
                float edgeFade = 1.0 - smoothstep(0.35, 0.5, max(centerDist.x, centerDist.y));
                float distanceFade = 1.0 - smoothstep(0.5, 1.0, float(i) / float(steps));
                
                vec3 reflColor = texture(colorTex, screenUV).rgb;
                float weight = sampleWeights[sample] * edgeFade * distanceFade;
                
                totalReflection += reflColor * weight;
                totalWeight += weight;
                break;
            }
        }
    }
    
    return totalWeight > 0.0 ? totalReflection / totalWeight : vec3(0.0);
}

// Alternative: Use normals for additional occlusion hints
vec3 NormalAwareSSR(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, sampler2D depthTex, sampler2D colorTex, sampler2D normalTex) {
    vec3 reflDir = reflect(-viewDir, normal);
    vec3 rayStart = worldPos + normal * 0.05;
    
    const int maxSteps = 75;
    const float maxDistance = 45.0;
    const float thickness = 1;
    
    vec4 viewRayStart = viewMatrix * vec4(rayStart, 1.0);
    vec4 viewReflDir = viewMatrix * vec4(reflDir, 0.0);
    
    vec3 rayStartView = viewRayStart.xyz;
    vec3 rayDirView = normalize(viewReflDir.xyz);
    
    float stepSize = maxDistance / float(maxSteps);
    
    for (int i = 1; i < maxSteps; i++) {
        vec3 currentViewPos = rayStartView + rayDirView * float(i) * stepSize;
        
        vec4 projPos = projection * vec4(currentViewPos, 1.0);
        if (projPos.w <= 0.0) break;
        
        vec2 screenUV = (projPos.xy / projPos.w) * 0.5 + 0.5;
        if (screenUV.x < 0.0 || screenUV.x > 1.0 || 
            screenUV.y < 0.0 || screenUV.y > 1.0) break;
        
        float sceneDepthNorm = texture(depthTex, screenUV).r;
        if (sceneDepthNorm >= 0.999) continue;
        
        float sceneDepth = NormalizedToViewDepth(sceneDepthNorm);
        float rayDepth = -currentViewPos.z;
        
        if (rayDepth > sceneDepth && rayDepth - sceneDepth < thickness) {
            // Additional check: surface normal orientation
            vec3 surfaceNormal = texture(normalTex, screenUV).xyz * 2.0 - 1.0;
            vec3 rayDirWorld = normalize(reflDir);
            
            // If the surface normal faces away from the ray, it's likely visible
            if (dot(surfaceNormal, -rayDirWorld) > 0.1) {
                float edgeFade = 1.0 - smoothstep(0.4, 0.5, max(abs(screenUV.x - 0.5), abs(screenUV.y - 0.5)));
                float distanceFade = 1.0 - (float(i) / float(maxSteps));
                
                vec3 reflColor = texture(colorTex, screenUV).rgb;
                return reflColor * edgeFade * distanceFade;
            }
        }
    }
    
    return vec3(0.0);
}

// Conservative SSR - only reflects objects that are definitely visible
vec3 ConservativeSSR(vec3 worldPos, vec3 viewDir, vec3 normal, mat4 viewProj, sampler2D depthTex, sampler2D colorTex) {
    vec3 reflDir = reflect(-viewDir, normal);
    vec3 rayStart = worldPos + normal * 0.1; // Larger offset
    
    const int maxSteps = 100;
    const float maxDistance = 30.0;
    const float thickness = 1;
    
    vec4 viewRayStart = viewMatrix * vec4(rayStart, 1.0);
    vec4 viewReflDir = viewMatrix * vec4(reflDir, 0.0);
    
    vec3 rayStartView = viewRayStart.xyz;
    vec3 rayDirView = normalize(viewReflDir.xyz);
    
    float stepSize = maxDistance / float(maxSteps);
    
    for (int i = 1; i < maxSteps; i++) {
        vec3 currentViewPos = rayStartView + rayDirView * float(i) * stepSize;
        
        vec4 projPos = projection * vec4(currentViewPos, 1.0);
        if (projPos.w <= 0.0) break;
        
        vec2 screenUV = (projPos.xy / projPos.w) * 0.5 + 0.5;
        
        // More conservative screen bounds
        if (screenUV.x < 0.1 || screenUV.x > 0.9 || 
            screenUV.y < 0.1 || screenUV.y > 0.9) {
            break;
        }
        
        float sceneDepthNorm = texture(depthTex, screenUV).r;
        if (sceneDepthNorm >= 0.999) continue;
        
        float sceneDepth = NormalizedToViewDepth(sceneDepthNorm);
        float rayDepth = -currentViewPos.z;
        
        // Very strict intersection test
        if (rayDepth > sceneDepth && rayDepth - sceneDepth < thickness) {
            // Only return reflections from the center area of the screen
            vec2 centerDist = abs(screenUV - 0.5);
            if (max(centerDist.x, centerDist.y) < 0.3) {
                return texture(colorTex, screenUV).rgb;
            }
        }
    }
    
    return vec3(0.0);
}

void main() {
    vec2 screenUV = (ClipSpacePos.xy / ClipSpacePos.w) * 0.5 + 0.5;
    
    // Get animated water normal
    vec3 waterNormal = normalize(Normal);
    
    // Add wave animation
    vec2 waveOffset = sin(FragPos.xz * 3.0 + time * 2.0) * 0.02;
    waterNormal.x += waveOffset.x;
    waterNormal.z += waveOffset.y;
    waterNormal = normalize(waterNormal);
    
    // Get view direction
    vec3 viewDir = normalize(cameraWorldPos - FragPos);
    
    // Use the balanced SSR method
    vec3 reflectionColor = MultiRaySSR(
        FragPos, viewDir, waterNormal, 
        viewProjection, gLinearDepth, gAlbedoSpec
    );

    // If still no good reflection, try the conservative approach
    if (length(reflectionColor) < 0.1) {
        reflectionColor = ConservativeSSR(
            FragPos, viewDir, waterNormal, 
            viewProjection, gLinearDepth, gAlbedoSpec
        );
    }
    
    // Get refraction with distortion
    vec2 distortedUV = screenUV + waterNormal.xz * 0.03;
    distortedUV = clamp(distortedUV, 0.0, 1.0);
    vec3 refractionColor = texture(gAlbedoSpec, distortedUV).rgb;
    
    // Calculate fresnel effect
    float fresnel = pow(1.0 - max(dot(viewDir, waterNormal), 0.0), 2.0);
    fresnel = clamp(fresnel, 0.1, 0.9);
    
    // Mix reflection and refraction
    vec3 mixedReflRefr = mix(refractionColor, reflectionColor, fresnel);
    vec3 finalColor = mix(mixedReflRefr, waterColor, 0.3);
    
    // Add sparkle effect
    float sparkle = sin(FragPos.x * 15.0 + time * 3.0) * sin(FragPos.z * 15.0 + time * 3.0);
    sparkle = sparkle * 0.1 + 0.9;
    finalColor *= sparkle;
    
    FragColor = vec4(finalColor, waterAlpha);
}