#version 330 core
out vec4 FragColor;

// Input from vertex shader
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

// Texture sampler
uniform sampler2D texture_diffuse1;

// Optional: Basic lighting
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main()
{
    // Sample the texture
    vec3 color = texture(texture_diffuse1, TexCoords).rgb;
    
    // Optional: Add simple lighting
    vec3 ambient = 0.15 * color;
    
    // Diffuse lighting
    vec3 lightColor = vec3(1.0);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 normal = normalize(Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Simple specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;
    
    // Combine lighting
    vec3 result = (ambient + diffuse + specular) * color;
    
    FragColor = vec4(result, 1.0);
}