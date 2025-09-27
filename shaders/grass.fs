#version 330 core

in vec2 FragTexCoord;
in vec3 FragWorldPos; 
in float GrassHeight;
in vec3 FragNormal;

uniform sampler2D grassTexture;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 cameraPos;

out vec4 FragColor;

void main()
{
    // Sample grass texture
    vec4 grassColor = texture(grassTexture, FragTexCoord);
    
    // Discard transparent pixels
    if (grassColor.a < 0.1)
        discard;
    
    // Basic lighting calculation
    vec3 normal = normalize(FragNormal);
    vec3 lightDir = normalize(-lightDirection);
    
    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Combine lighting
    vec3 ambient = ambientColor;
    vec3 result = (ambient + diffuse) * grassColor.rgb;
    
    // Add some height-based color variation
    float heightFactor = GrassHeight / 2.0; // Assuming max height of 2
    result *= mix(vec3(0.3, 0.6, 0.2), vec3(0.4, 0.8, 0.3), heightFactor);
    
    FragColor = vec4(result, grassColor.a);
}