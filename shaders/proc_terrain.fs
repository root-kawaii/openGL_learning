#version 330 core

in vec3 gFragPos;
in vec3 gNormal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// --- Colors sampled directly from your reference ---
const vec3 INK_COLOR  = vec3(0.01, 0.05, 0.10); // The dark navy "black" lines
const vec3 DUNE_LOW   = vec3(0.12, 0.25, 0.41); // Deep shadowed teal
const vec3 DUNE_MID   = vec3(0.24, 0.44, 0.61); // Main surface blue
const vec3 DUNE_HIGH  = vec3(0.44, 0.68, 0.81); // Highlighted ridge teal

// --- Style Settings ---
const float BANDS     = 3.0;   // Very few levels for that flat look
const float EDGE_SOFT = 0.02;  // How "crisp" the ink lines are
const float LINE_THICKNESS = 0.45; // Higher = thicker black lines

void main()
{
    vec3 norm = normalize(gNormal);
    vec3 viewDir = normalize(viewPos - gFragPos);

    // 1. Height & Slope Color Blend
    // In the reference, color follows both height and the "fold" of the terrain
    float heightT = clamp((gFragPos.y + 1.0) / 10.0, 0.0, 1.0);
    vec3 baseColor = mix(DUNE_LOW, DUNE_MID, heightT);
    
    // 2. Extremely Simple Toon Shading
    // The reference is very flat, so we don't want complex light math
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); // Fixed "Moon" direction
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Snap the lighting to very few levels
    float toon = floor(diff * BANDS) / BANDS;
    vec3 finalSurface = mix(baseColor, DUNE_HIGH, toon);

    // 3. The "Inked" Outlines (Fresnel Inking)
    // This looks for the edges of the 3D model relative to the camera
    float edgeDetect = max(dot(viewDir, norm), 0.0);
    
    // If the surface is facing away from the camera, it becomes the INK_COLOR
    float outlineMask = smoothstep(LINE_THICKNESS, LINE_THICKNESS + EDGE_SOFT, edgeDetect);

    // 4. Final Composition
    // Apply the ink mask to the surface color
    vec3 finalColor = mix(INK_COLOR, finalSurface, outlineMask);

    // Optional: Subtle Posterization for that low-bit look
    finalColor = floor(finalColor * 12.0) / 12.0;

    FragColor = vec4(finalColor, 1.0);
}