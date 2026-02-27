#version 330 core
out vec4 FragColor;

in vec3 TexCoords;   // direction vector from the cube vertices

// ---------------------------------------------------------------------------
// Shared procedural sky function.
// dir  — world-space direction to shade (need not be normalised here).
// Returns HDR-ish RGB sky colour.
// ---------------------------------------------------------------------------
vec3 proceduralSky(vec3 dir)
{
    dir = normalize(dir);
    float y = dir.y;   // -1 (nadir) … +1 (zenith)

    // --- gradient ---
    vec3 zenith  = vec3(0.10, 0.04, 0.28);   // deep indigo/violet at zenith
    vec3 midSky  = vec3(0.38, 0.10, 0.52);   // rich purple mid-sky
    vec3 horizon = vec3(0.95, 0.38, 0.28);   // warm orange-red at horizon
    vec3 ground  = vec3(0.05, 0.02, 0.18);   // dark purple below horizon (ocean)

    vec3 sky;
    if (y >= 0.0) {
        float t = pow(y, 0.55);
        sky = mix(horizon, mix(midSky, zenith, t), t);
    } else {
        sky = mix(ground, horizon, clamp(1.0 + y * 5.0, 0.0, 1.0));
    }

    // --- sun disc + glow (low on the horizon — sunset) ---
    vec3 sunDir = normalize(vec3(0.55, 0.08, 0.40));

    float sd = dot(dir, sunDir);

    // tight disc — pale yellow-white
    sky += vec3(1.00, 0.95, 0.80) * smoothstep(0.9993, 1.000, sd);
    // inner corona — deep orange
    sky += vec3(1.00, 0.55, 0.15) * smoothstep(0.988, 0.9993, sd) * 0.80;
    // mid glow — magenta/pink
    sky += vec3(0.85, 0.25, 0.50) * smoothstep(0.950, 0.988,  sd) * 0.45;
    // outer glow — wide purple haze
    sky += vec3(0.45, 0.10, 0.55) * smoothstep(0.880, 0.950,  sd) * 0.25;

    // --- horizon warmth band on sun side ---
    float horizonBand = exp(-abs(y) * 4.5);
    float sunXZ = clamp(dot(normalize(dir.xz), normalize(sunDir.xz)), 0.0, 1.0);
    sky += vec3(1.00, 0.45, 0.10) * horizonBand * sunXZ * 0.40;

    // --- opposite horizon: cool purple twilight ---
    float antiSunXZ = clamp(dot(normalize(dir.xz), -normalize(sunDir.xz)), 0.0, 1.0);
    sky += vec3(0.20, 0.05, 0.40) * horizonBand * antiSunXZ * 0.20;

    return sky;
}

void main()
{
    FragColor = vec4(proceduralSky(TexCoords), 1.0);
}
