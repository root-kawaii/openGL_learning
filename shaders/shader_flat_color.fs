#version 330 core
out vec4 FragColor;

// Input from vertex shader
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

// Color uniform
uniform vec4 objectColor;
// Or use vec4 if you want alpha support:
// uniform vec4 objectColor;

// Optional: Lighting uniforms
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main()
{
    // Option 1: Simple solid color (no lighting)
    // FragColor = vec4(objectColor, 1.0);
    
    // Option 2: Color with basic lighting
    vec4 color = objectColor;
    
    // Ambient lighting
    vec3 ambient = 0.15 * color.rgb;
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular lighting
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * lightColor;
    
    // Combine all lighting
    vec3 result = (ambient + diffuse + specular) * color.rgb;
    FragColor = vec4(result, 1.0);
    
    // For vec4 color uniform with alpha:
    // FragColor = vec4(result, objectColor.a);
}