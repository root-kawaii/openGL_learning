# Terrain Shader - Known Issues & Potential Improvements

## Current Issues

### 1. Side face stretching/streaking
The cube side faces (thin vertical strips) show horizontal line artifacts.
- **Cause**: The triplanar blend on side faces uses worldPos.yz or worldPos.xy as UVs, but the side faces of a unit cube are only 1 unit tall — with texTiling=0.25 they only sample a tiny strip of the texture, causing visible stretching.
- **Fix**: Use a higher tiling scale specifically for side faces (e.g. `uv * texTiling * 4.0` for sides), or use a separate smaller cliff texture that tiles well at that scale. Could also just use the flat objectColor for sides since they're barely visible from top-down.

### 2. Hard seams between terrain types
Where a grass tile meets a stone tile, there's a sharp color boundary at the tile edge.
- **Fix**: Pass neighbor terrain types as uniforms (or encode in a texture/SSBO) and blend at edges. In the fragment shader, detect proximity to tile border using LocalPos, then sample both the current tile's material and the neighbor's material and lerp between them over a small border region (~0.1 units).

### 3. Normal map TBN on flat cube faces
The cube.obj likely has axis-aligned normals with degenerate tangent/bitangent on some faces (tangent may be zero or parallel to normal).
- **Fix**: For triplanar mapping, compute the TBN per-axis in the fragment shader instead of relying on mesh tangents. Use the triplanar projection axis to derive T and B analytically:
```glsl
// For Y-projection (top face):
vec3 T = vec3(1, 0, 0);
vec3 B = vec3(0, 0, 1);
vec3 N = vec3(0, 1, 0);
mat3 topTBN = mat3(T, B, N);
```

## Future Improvements

### 4. Height-based texture blending
Instead of hard terrain_type per tile, blend two materials based on a height map. Rock pokes through grass where the height map says rock is tall:
```glsl
float rockH = texture(rockHeightMap, uv).r;
float grassH = texture(grassHeightMap, uv).r;
float blend = smoothstep(0.0, 0.1, rockH - grassH + bias);
color = mix(grassCol, rockCol, blend);
```
This gives the natural look where grass fills crevices and rock sticks up.

### 5. Parallax Occlusion Mapping (POM)
Use the displacement maps (already converted in assets/terrain/*_disp.jpg) to offset UVs based on view angle, creating the illusion of depth on flat surfaces. Most impactful at grazing angles.

### 6. Instanced rendering for tiles
Currently each tile is a separate draw call with its own shader setup. Could batch all tiles of the same terrain type into instanced draws, passing terrainType and position via instance attributes. Would significantly reduce draw calls (100 -> ~4).

### 7. Texture atlas instead of branching
Replace the `if (terrainType == 0)` branches with a texture array or atlas. Pack all terrain diffuse/normal maps into a GL_TEXTURE_2D_ARRAY and index by terrainType — eliminates shader branching and allows the GPU to batch better.

### 8. Ambient Occlusion map
Use the roughness/AO maps from the PBR packs to modulate ambient lighting per-texel, adding depth to crevices.

### 9. Detail texture overlay
Add a small tiling detail texture (noise, cracks) that overlays at a higher frequency to add close-up detail without needing enormous base textures.

### 10. Decals / splat overlay
For things like path markings, scuff marks, or painted lines on the playing field — render as projected decals on top of the terrain rather than baking into the terrain type system.
