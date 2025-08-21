#version 330 core

in vec3 worldPos;
in vec3 viewPos;

out vec4 FragColor;

// Hardcoded grid settings
const float gridScale = 1.0;           // 1 unit grid cells
const float lineWidth = 0.5;           // Line thickness
const int majorLineInterval = 10;      // Major line every 10 units
const float majorLineWidth = 0.7;      // Major line thickness
const float axisWidth = 5.0;           // Axis line thickness
const float fadeDistance = 800.0;       // Distance where grid fades out
const float nearFadeDistance = 0.5;    // Near fade to prevent artifacts

// Colors
const vec3 gridColor = vec3(0.9, 0.9, 0.9);        // Regular grid lines
const vec3 majorLineColor = vec3(0.5, 0.5, 0.5);   // Major grid lines
const vec3 xAxisColor = vec3(0.8, 0.3, 0.3);       // X-axis (red)
const vec3 zAxisColor = vec3(0.3, 0.3, 0.8);       // Z-axis (blue)

float getGridLine(vec2 coord, float lineWidth, float scale) {
    vec2 grid = abs(fract(coord / scale - 0.5) - 0.5) / fwidth(coord / scale);
    float line = min(grid.x, grid.y);
    return 1.0 - min(line / lineWidth, 1.0);
}

void main()
{
    vec2 coord = worldPos.xz;
    
    // Calculate distance fading
    float distance = length(viewPos);
    float farFade = 1.0 - smoothstep(0.0, fadeDistance, distance);
    float nearFade = smoothstep(0.0, nearFadeDistance, distance);
    float fade = farFade * nearFade * farFade; // Extra falloff for better look
    
    // Get regular grid lines
    float gridLine = getGridLine(coord, lineWidth, gridScale);
    
    // Get major grid lines
    float majorLine = getGridLine(coord, majorLineWidth, gridScale * float(majorLineInterval));
    
    // Get axis lines
    float xAxis = 1.0 - min(abs(coord.y) / fwidth(coord.y) / axisWidth, 1.0);
    float zAxis = 1.0 - min(abs(coord.x) / fwidth(coord.x) / axisWidth, 1.0);
    
    // Combine colors with proper priority (axes > major > regular)
    vec3 finalColor = vec3(0.0);
    float totalAlpha = 0.0;
    
    // Apply grid lines
    if (gridLine > 0.0) {
        finalColor = gridColor;
        totalAlpha = gridLine * fade;
    }
    
    // Apply major lines (override grid lines)
    if (majorLine > 0.0) {
        finalColor = majorLineColor;
        totalAlpha = majorLine * fade;
    }
    
    // Apply axis lines (highest priority)
    if (xAxis > 0.0) {
        finalColor = xAxisColor;
        totalAlpha = max(totalAlpha, xAxis);
    }
    if (zAxis > 0.0) {
        finalColor = zAxisColor;
        totalAlpha = max(totalAlpha, zAxis);
    }
    
    // Only output fragments where we have visible lines
    if (totalAlpha < 0.01) {
        discard;  // Don't render anything in empty grid squares
    }
    
    FragColor = vec4(finalColor, totalAlpha);
}
