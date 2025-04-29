#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

// const float offset = 1.0 / 300.0;  

// #version 330 core
// out vec4 FragColor;

// in vec3 TexCoords;

// uniform samplerCube skybox;

void main()
{    
    FragColor = texture(texture_diffuse1, TexCoords);
}

// void main()
// {    
//     vec2 offsets[9] = vec2[](
//         vec2(-offset,  offset), // top-left
//         vec2( 0.0f,    offset), // top-center
//         vec2( offset,  offset), // top-right
//         vec2(-offset,  0.0f),   // center-left
//         vec2( 0.0f,    0.0f),   // center-center
//         vec2( offset,  0.0f),   // center-right
//         vec2(-offset, -offset), // bottom-left
//         vec2( 0.0f,   -offset), // bottom-center
//         vec2( offset, -offset)  // bottom-right    
//     );

//     float kernel[9] = float[](
//         -1, -1, -1,
//         -1,  9, -1,
//         -1, -1, -1
//     );
    
//     vec3 sampleTex[9];
//     vec2 flippedTexCoord = vec2(1.0 - TexCoords.x, TexCoords.y);  // Flip Y-coordinate
//     for(int i = 0; i < 9; i++)
//     {
//         flippedTexCoord = flippedTexCoord + offsets[i];
//         sampleTex[i] = vec3(texture(texture_diffuse1, flippedTexCoord));
//     }
//     vec3 col = vec3(0.0);
//     for(int i = 0; i < 9; i++)
//         col += sampleTex[i] * kernel[i];
    
//     FragColor = vec4(col, 1.0);
// }