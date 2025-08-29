#version 330 core
in vec2 FragTexCoord;
in vec3 FragWorldPos;
in float GrassHeight;

uniform sampler2D grassTexture;
uniform vec3 grassColor;
uniform vec3 grassTipColor;
uniform float alphaThreshold;
uniform vec3 lightDir;

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(grassTexture, FragTexCoord);
    
    // Create procedural alpha if texture doesn't have it
    float proceduralAlpha = 1.0;
    
    // Make edges more transparent for softer look
    float edgeFade = 1.0 - abs(FragTexCoord.x - 0.5) * 2.0; // Fade at sides
    edgeFade = smoothstep(0.0, 0.5, edgeFade);
    
    // Taper towards top
    float topTaper = 1.0 - pow(GrassHeight, 2.0);
    topTaper = max(topTaper, 0.1); // Don't fade completely
    
    proceduralAlpha = edgeFade * topTaper;
    
    // Combine with texture alpha
    float finalAlpha = 1;
    
    if (finalAlpha < alphaThreshold) {
        discard;
    }
    
    // Natural color variation
    float heightFactor = smoothstep(0.0, 1.0, GrassHeight);
    vec3 blendedColor = mix(grassColor, grassTipColor, heightFactor);
    
    // Simple lighting with subsurface scattering
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = max(dot(vec3(0, 1, 0), lightDirection), 0.0);
    float subsurface = max(dot(vec3(0, 1, 0), -lightDirection), 0.0) * 0.3;
    float lightFactor = NdotL + subsurface + 0.4; // Ambient
    
    vec3 finalColor = texColor.rgb * blendedColor * lightFactor;
    
    // Ambient occlusion at base
    float ao = mix(0.8, 1.0, pow(GrassHeight, 0.5));
    finalColor *= ao;
    
    FragColor = vec4(finalColor, finalAlpha);
}