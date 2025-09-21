#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec4 color;
uniform bool useTexture;
uniform sampler2D texture1;

void main()
{
    if (useTexture) {
        vec4 texColor = texture(texture1, TexCoords);
        FragColor = texColor * color;
    } else {
        FragColor = color;
    }
}