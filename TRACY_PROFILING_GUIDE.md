# Tracy Profiler Guide for OpenGL Performance Analysis

## Quick Start

### Step 1: Run Tracy Profiler
```bash
cd /home/monolith/Desktop/openGL_learning
./run_tracy_profiler.sh
```

### Step 2: Run Your Application
In another terminal:
```bash
cd /home/monolith/Desktop/openGL_learning
./build/hephaestus-openGL
```

### Step 3: Tracy Will Auto-Connect
- Tracy profiler GUI will show "Connection established"
- Real-time profiling data will start appearing

### Step 4: Capture Performance Data
- Click **"Capture"** button in Tracy to save a snapshot
- Play with your scene for 5-10 seconds to capture varied data
- Saved traces go to Tracy's data directory

---

## What's Already Instrumented

Your code already has Tracy zones in these critical functions:

### Main Render Loop
- ✅ `renderShadowPass()` - Shadow map rendering
- ✅ `renderMainPass()` - Main scene rendering
- ✅ `renderSceneToIDBuffer()` - ID buffer for mouse picking

### Per-Object Rendering
- ✅ `renderGameObject()` - Individual object rendering
- ✅ `renderGameObjectWithShader()` - Custom shader rendering

### Utility Rendering
- ✅ `renderQuad()` - Quad rendering
- ✅ `renderCube()` - Cube rendering
- ✅ `renderInfiniteGrid()` - Grid rendering
- ✅ `renderGrass()` - Grass rendering

### Main Loop
- ✅ `FrameMark` - Frame boundaries (in main.cpp:244)
- ✅ `Game::update()` - Game logic updates

---

## Key Metrics to Look For

### 1. **Frame Time Breakdown**
Look for which zones take the most time:
- `renderShadowPass` - Should be faster after frustum culling optimization
- `renderMainPass` - Main rendering, will show object draw times
- `renderSceneToIDBuffer` - Should only appear on first frame and mouse clicks now!

### 2. **GPU vs CPU Bottlenecks**
- **CPU-bound**: Zones show significant time, GPU idle
- **GPU-bound**: Zones complete quickly, but frame time is high

### 3. **Draw Call Patterns**
- Look at `renderGameObject` zones - each call represents one object
- Check for clustering (good) vs spread out (bad)

### 4. **Optimization Verification**

**Before optimizations, you should have seen:**
- `renderSceneToIDBuffer` EVERY frame (wasting 20-40% time)
- `renderParabolicTrajectory` with 1000 segments every frame
- `checkAndReloadShaders` filesystem calls every second

**After optimizations, you should see:**
- `renderSceneToIDBuffer` only on first frame + mouse clicks
- `renderParabolicTrajectory` not appearing (disabled by default)
- `checkAndReloadShaders` only in ENGINE mode

---

## Expected Performance Profile

### Healthy Frame Breakdown (60 FPS = 16.67ms per frame)
```
Total Frame:        ~8-12ms (127 FPS ✓)
├─ renderShadowPass:   ~1-2ms  (with frustum culling)
├─ renderMainPass:     ~5-8ms  (drawing 2.1M triangles)
│  ├─ space.glb:       ~2-4ms  (500k+ triangles - HEAVY!)
│  └─ mech_drones:     ~2-3ms  (5x 200k+ triangles - HEAVY!)
├─ renderSceneToIDBuffer: 0ms  (only on clicks)
└─ ImGui overhead:     ~0.5-1ms
```

### Problem Areas to Watch

**🔴 Red Flags:**
- `renderSceneToIDBuffer` appearing every frame → optimization not working
- Any single `renderGameObject` taking >2ms → model too high-poly
- `renderShadowPass` >3ms → frustum culling not helping enough
- Uniform lookups showing up → caching not working

**🟡 Yellow Flags:**
- Total frame time >16.67ms → dropping below 60 FPS
- Spikes in frame time → GC pauses or asset loading

**🟢 Good Signs:**
- Consistent frame times
- `renderSceneToIDBuffer` only on clicks
- Shadow pass culling objects outside frustum
- Uniform caching eliminating `glGetUniformLocation` calls

---

## Advanced Analysis

### Flame Graph View
1. Switch to "Frames" view in Tracy
2. Click on a frame to see detailed breakdown
3. Look for the tallest bars = biggest bottlenecks

### Statistics View
1. Go to "Statistics" tab
2. Sort by "Total time" to see which zones consume most CPU
3. Check "Mean time" for per-call averages

### Find View
1. Press Ctrl+F
2. Search for specific zones like "renderGameObject"
3. See all occurrences and their timing

---

## Bottleneck Analysis Guide

### Suspected Bottleneck: High-Poly Models

**What to look for:**
- Individual `renderGameObject` calls taking >2ms
- `Model::Draw` showing long times

**Confirm with:**
```bash
# Check console output when app starts - it now prints triangle counts
grep "Total triangles" /tmp/app_output.log
```

**Solution:**
- Decimate models in Blender (target 20k-50k triangles)
- Implement LOD (Level of Detail) system
- Use GPU instancing for repeated models

### Suspected Bottleneck: Excessive Draw Calls

**What to look for:**
- Many small `renderGameObject` zones
- Total time spread across many calls

**Solution:**
- Batch similar objects
- Use instanced rendering
- Merge static geometry

### Suspected Bottleneck: Shadow Map Rendering

**What to look for:**
- `renderShadowPass` taking >3ms
- Similar object count as main pass

**Solution:**
- Lower shadow map resolution (1024 → 512)
- More aggressive frustum culling
- Reduce far plane distance

---

## Optimization Verification Checklist

After running Tracy, verify these optimizations are working:

- [ ] `renderSceneToIDBuffer` only appears once at startup
- [ ] `renderSceneToIDBuffer` appears when you click the mouse
- [ ] `renderParabolicTrajectory` does NOT appear (trajectory disabled)
- [ ] If trajectory enabled, only uses 50 segments not 1000
- [ ] `checkAndReloadShaders` only in ENGINE mode (not GAME mode)
- [ ] Shadow frustum culling reduces objects rendered in shadow pass
- [ ] No `glGetUniformLocation` symbols visible (uniform caching working)

---

## Troubleshooting

### Tracy Won't Connect
1. Make sure both Tracy profiler and your app are running
2. Check firewall isn't blocking port 8086
3. Verify Tracy library is linked (should auto-connect)

### No Zones Visible
1. Check that `TRACY_ENABLE` is defined during compilation
2. Verify `ZoneScoped` macros are not commented out
3. Make sure `FrameMark` is called each frame

### Performance Worse Than Expected
1. Run in Release mode, not Debug
2. Disable VSync if testing max FPS
3. Check GPU isn't thermal throttling
4. Verify model triangle counts (should see in console now)

---

## Next Steps After Profiling

Based on Tracy data, you'll likely want to:

1. **Reduce space.glb complexity** (currently ~500k-1M triangles)
   - Target: 10k-50k triangles

2. **Reduce mech_drone.glb complexity** (currently ~200k-400k each × 5)
   - Target: 20k-50k triangles each

3. **Implement GPU instancing** for the 5 identical mech drones
   - Reduces 5 draw calls to 1

4. **Add LOD system** for distance-based detail reduction

Expected result: **300-500+ FPS** (currently 127 FPS)

---

## Save and Share Traces

To save a trace:
1. Click "Capture" in Tracy
2. Let it record for 5-10 seconds
3. Click "Save" - creates a `.tracy` file
4. Open later with Tracy for offline analysis

Trace files are useful for:
- Before/after optimization comparisons
- Sharing performance data
- Regression testing

---

## Summary

Tracy is already fully integrated! Just run:
1. `./run_tracy_profiler.sh`
2. Run your OpenGL app
3. Analyze the real-time performance data

The profiler will show you exactly where time is being spent and confirm that all 5 optimizations are working correctly.
