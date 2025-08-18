#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D smokeTexture;
uniform float alpha;
uniform float time; // New uniform for animation time

void main() {
    // Offset the texture coordinates to make the smoke "move"
    // The direction and speed of movement can be controlled here.
    // vec2 animatedTexCoords = TexCoords + vec2(0.0, -time * 0.1); // Scrolls down
    
    // A more advanced effect: circular or swirling motion
    float swirl = sin(time * 0.5) * 0.1;
    vec2 animatedTexCoords = TexCoords + vec2(swirl, -time * 0.05);

    // Another advanced effect: Add a 'wiggle' to the texture coordinates
    vec2 wiggledTexCoords = TexCoords + vec2(sin(time * 2.0 + TexCoords.y) * 0.02, cos(time * 1.5 + TexCoords.x) * 0.02);
    
    // For a simple, consistent smoke effect, stick with a basic scroll
    vec2 finalTexCoords = TexCoords + vec2(0.0, -time * 0.05);

    vec4 texColor = texture(smokeTexture, finalTexCoords);
    
    // Check if the texture is a pure grayscale or color image. If it has no alpha,
    // you might need to use a single channel for the effect.
    // For smoke, we usually assume the texture's red channel contains density.
    // FragColor = vec4(texColor.rgb, texColor.r * alpha); 
    
    // Since your texture has an alpha channel, this is fine
    FragColor = vec4(texColor.rgb, texColor.a * alpha);
}