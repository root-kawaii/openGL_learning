#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

void main()
{    
    vec4 texColor = texture(texture_diffuse1, TexCoords);
    // texColor.a = 100;
    FragColor = vec4(texColor.rgb, 1.0);  // Keep color, override alpha

}