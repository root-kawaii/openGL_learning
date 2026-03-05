# Animation System Flow Documentation

## Overview
The animation system supports multiple animations per model with smooth blending, playback control, and event callbacks. It uses Assimp's native animation data and maintains frame-independent updates.

---

## Core Data Structures

### AnimationState
Tracks the state of a single animation:
- `animationIndex`: Index into scene->mAnimations[]
- `currentTime`: Current playback position (in animation ticks)
- `playbackSpeed`: Speed multiplier (1.0 = normal speed)
- `isLooping`: Whether animation loops
- `isPaused`: Whether animation is paused

### BlendState
Manages smooth transitions between animations:
- `isBlending`: Whether currently blending
- `sourceAnim`: Animation being blended from
- `targetAnim`: Animation being blended to
- `blendFactor`: Current blend progress (0.0 to 1.0)
- `blendDuration`: Total blend duration in seconds
- `blendElapsed`: Time elapsed during blend

### AnimationEvent
Callback system for animation events:
- `animationName`: Which animation this event belongs to
- `triggerTime`: When to trigger (in seconds)
- `callback`: Function to call
- `hasTriggered`: Whether already triggered this loop

---

## Main Animation Flow

```
Game Loop (every frame)
    ↓
GameObject::Draw()
    ↓
Model::Draw()
    ↓
Model::GetBoneTransforms(transforms, glfwGetTime())
    ↓
    Calculate deltaTime from timeSeconds
    ↓
    UpdateAnimationState(deltaTime)
    ↓
    GetBoneTransformsInternal(transforms)
    ↓
    Pass transforms to shader
```

---

## UpdateAnimationState() Flow

### Case 1: Currently Blending

```
UpdateAnimationState(deltaTime)
    ↓
Check if m_BlendState.isBlending == true
    ↓
Update blend progress:
    - blendElapsed += deltaTime
    - blendFactor = min(1.0, blendElapsed / blendDuration)
    ↓
Advance both animations:
    - UpdateSingleAnimationTime(sourceAnim, deltaTime)
    - UpdateSingleAnimationTime(targetAnim, deltaTime)
    ↓
Check if blend complete (blendFactor >= 1.0):
    - YES → Switch to target animation
           m_CurrentAnimation = targetAnim
           isBlending = false
    - NO  → Continue blending next frame
```

### Case 2: Not Blending - Check for Loop Blend

```
UpdateAnimationState(deltaTime)
    ↓
Check if NOT blending AND isLooping AND has animation
    ↓
Calculate time until animation end:
    - Get animation duration from scene->mAnimations[index]
    - timeUntilEnd = duration - currentTime
    ↓
Check if close to end (timeUntilEnd <= 0.3 seconds in ticks):
    - YES → Start loop blend:
            * Setup blend from current position to beginning
            * sourceAnim = current state
            * targetAnim = current state but currentTime = 0.0
            * blendDuration = remaining time
            * isBlending = true
            * Return (next frame handles blend)
    - NO  → Continue to normal time update
    ↓
Store previous time
    ↓
UpdateSingleAnimationTime(m_CurrentAnimation, deltaTime)
    ↓
CheckAnimationEvents(previousTime, currentTime)
```

---

## GetBoneTransformsInternal() Flow

### Case 1: Blending Active

```
GetBoneTransformsInternal(transforms)
    ↓
Check if m_BlendState.isBlending == true
    ↓
Compute source animation transforms:
    - ComputeBoneTransformsForAnimation(
        sourceTransforms,
        sourceAnim.animationIndex,
        sourceAnim.currentTime)
    ↓
Compute target animation transforms:
    - ComputeBoneTransformsForAnimation(
        targetTransforms,
        targetAnim.animationIndex,
        targetAnim.currentTime)
    ↓
BlendBoneTransforms(
    sourceTransforms,
    targetTransforms,
    blendFactor,
    transforms) → Output
```

### Case 2: No Blending

```
GetBoneTransformsInternal(transforms)
    ↓
Check if NOT blending
    ↓
ComputeBoneTransformsForAnimation(
    transforms,
    m_CurrentAnimation.animationIndex,
    m_CurrentAnimation.currentTime)
    ↓
Return transforms
```

---

## ComputeBoneTransformsForAnimation() Flow

```
ComputeBoneTransformsForAnimation(transforms, animIndex, animTime)
    ↓
Validate animation exists
    ↓
Get animation pointer: scene->mAnimations[animIndex]
    ↓
Create copy of m_BoneInfo
    ↓
ReadNodeHierarchyForAnimation(
    animTime,
    boneInfo,
    scene->mRootNode,
    m_globalInverseTransform,
    animation)
    ↓
    Recursively traverse skeleton:
        - Find bone's channel in animation
        - Interpolate position, rotation, scale at animTime
        - Build local transform matrix
        - Multiply with parent transform
        - If bone exists: compute final = inverse bind pose * transform
        - Recurse for all children
    ↓
Collect all bone transforms into output vector
```

---

## BlendBoneTransforms() Flow

```
BlendBoneTransforms(t1, t2, blendFactor, out)
    ↓
For each bone (min of both transform counts):
    ↓
    Extract Translation:
        - pos1 = t1[i][3]
        - pos2 = t2[i][3]
    ↓
    Extract Scale (length of basis vectors):
        - scale1 = length(t1[i][0], t1[i][1], t1[i][2])
        - scale2 = length(t2[i][0], t2[i][1], t2[i][2])
    ↓
    Extract Rotation (normalize to remove scale):
        - rotMat1 = mat3(t1[i]) normalized by scale1
        - rotMat2 = mat3(t2[i]) normalized by scale2
        - rot1 = quat_cast(rotMat1)
        - rot2 = quat_cast(rotMat2)
    ↓
    Interpolate:
        - finalPos = mix(pos1, pos2, blendFactor)
        - finalRot = slerp(rot1, rot2, blendFactor)  ← Quaternion spherical interpolation
        - finalScale = mix(scale1, scale2, blendFactor)
    ↓
    Reconstruct Transform Matrix:
        - Translate by finalPos
        - Rotate by finalRot
        - Scale by finalScale
    ↓
    Add to output vector
```

---

## UpdateSingleAnimationTime() Flow

```
UpdateSingleAnimationTime(animState, deltaTime)
    ↓
Validate animation exists
    ↓
Get ticks per second from animation (default: 25.0)
    ↓
Advance time:
    - deltaTimeTicks = deltaTime * ticksPerSecond * playbackSpeed
    - currentTime += deltaTimeTicks
    ↓
Check if looping:
    - YES → currentTime = fmod(currentTime, duration)
    - NO  → If currentTime > duration:
              * Clamp to duration
              * Set isPaused = true (auto-pause at end)
```

---

## User API Flows

### Playing an Animation

```
User calls: model.PlayAnimation("Walk", 0.3f)
    ↓
Look up animation index from name map
    ↓
PlayAnimationByIndex(index, 0.3f)
    ↓
Validate animation exists
    ↓
Check if already playing this animation:
    - YES → Return (do nothing)
    - NO  → Continue
    ↓
Check blend duration:
    - <= 0 OR no current animation:
        * Instant switch
        * m_CurrentAnimation = new animation at time 0
    - > 0:
        * Setup blend:
            - sourceAnim = current animation state
            - targetAnim = new animation at time 0
            - blendDuration = requested duration
            - blendElapsed = 0
            - blendFactor = 0
            - isBlending = true
```

### Registering Animation Event

```
User calls: model.RegisterAnimationEvent("Attack", 0.5f, callback)
    ↓
Create AnimationEvent:
    - animationName = "Attack"
    - triggerTime = 0.5 seconds
    - callback = user function
    - hasTriggered = false
    ↓
Add to m_AnimationEvents vector
    ↓
Events checked every frame in UpdateAnimationState():
    - If animation time crosses trigger time
    - AND event hasn't triggered yet
    - Call callback()
    - Mark hasTriggered = true
```

---

## Initialization Flow

```
Model constructor
    ↓
loadModel(path)
    ↓
    Assimp imports file
    ↓
    processNode() - builds meshes
    ↓
    InitializeAnimations()
        ↓
        Check if scene has animations:
            - NO  → Set m_AnimationsInitialized = true, return
            - YES → Continue
        ↓
        Build animation name-to-index map:
            - For each animation in scene->mAnimations[]
            - Map animation name → index
            - Print "Registered animation: 'name' (index i)"
        ↓
        Set default animation:
            - m_CurrentAnimation.animationIndex = 0
            - currentTime = 0
            - isLooping = true
            - playbackSpeed = 1.0
            - isPaused = false
        ↓
        Set m_AnimationsInitialized = true
```

---

## Key Timing Details

### Time Units
- **deltaTime**: Seconds (from frame to frame)
- **currentTime**: Animation ticks (internal unit)
- **duration**: Animation ticks (from aiAnimation)
- **ticksPerSecond**: Conversion factor (typically 25.0 or from animation)
- **Event times**: Seconds (user-facing)

### Conversions
```cpp
// Seconds to ticks
float ticks = seconds * ticksPerSecond;

// Ticks to seconds
float seconds = ticks / ticksPerSecond;
```

### Loop Blend Threshold
- Starts blending **0.3 seconds** before animation end
- Blend duration = remaining time until end
- Ensures seamless loop without visible pop

---

## Special Cases

### Models Without Animations
- `InitializeAnimations()` sets `m_AnimationsInitialized = true` anyway
- `ComputeBoneTransformsForAnimation()` returns empty transforms
- No crashes, just no bone transforms applied

### Animation Without Blending
- User calls `PlayAnimation("Walk", 0.0f)` with zero blend duration
- Instant switch to new animation
- No blending state activated

### Paused Animations
- `UpdateAnimationState()` returns early if paused
- Time doesn't advance
- Blending doesn't progress
- Events don't fire

### Non-Looping Animations
- When time reaches duration, clamps to duration
- Sets `isPaused = true` automatically
- Stays on last frame
- No loop blend occurs

---

## Performance Considerations

### Blending Cost
- Blending requires computing **two** full skeleton transforms per frame
- `BlendBoneTransforms()` processes every bone (typically 50-100+)
- Uses quaternion slerp (more expensive than lerp)
- Only active during blend duration (typically 0.3-0.5 seconds)

### Optimization Opportunities
- Bone transform computation is cached in `m_BoneInfo`
- Skeleton traversal happens once per animation evaluation
- No runtime allocations during playback (vectors pre-reserved)

### Memory Usage
- One `AnimationState` for current animation (small)
- One `BlendState` for blending (contains two AnimationStates)
- Event vector grows with registered events
- Bone transform vectors sized to bone count

---

## Common Usage Patterns

### Simple Playback
```cpp
// Load model with animations
Model model("character.glb");

// Play animation by name
model.PlayAnimation("Idle");

// In game loop - animations update automatically
model.Draw(shader);
```

### Animation Switching with Blending
```cpp
// Smooth transition
model.PlayAnimation("Run", 0.3f);  // 0.3 second blend

// Instant switch
model.PlayAnimation("Death", 0.0f);  // No blend
```

### Event-Driven Gameplay
```cpp
// Register attack impact event
model.RegisterAnimationEvent("Attack", 0.5f, []() {
    ApplyDamage(enemy);
    PlaySound("impact.wav");
});

// Event fires automatically when animation reaches 0.5 seconds
```

### Playback Control
```cpp
// Slow motion
model.SetAnimationSpeed(0.5f);

// Pause
model.PauseAnimation();

// Resume
model.ResumeAnimation();

// Seek to specific time
model.SetAnimationTime(2.5f);  // Jump to 2.5 seconds

// Disable looping
model.SetAnimationLooping(false);
```

---

## Error Handling

### Missing Animation
```cpp
model.PlayAnimation("NonExistent");
// Prints: "Animation 'NonExistent' not found!"
// Current animation continues playing
```

### Invalid Animation Index
```cpp
// ComputeBoneTransformsForAnimation validates:
if (!scene || !scene->mAnimations || animIndex >= scene->mNumAnimations) {
    return;  // Empty transforms
}
```

### Event on Wrong Animation
```cpp
// Events only fire when their animationName matches current animation
// Events for other animations are ignored
```

---

## Performance Optimization History

This section documents the optimization passes applied to skeletal animation, starting from the baseline and ending with the current parallel implementation.

### Baseline — ~220 µs per frame

The original `Draw()` computed bone transforms inline every frame for every draw call.

**Problems:**
- `ReadNodeHierarchyForAnimation()` walked the scene node tree recursively, calling `std::string` comparisons at every node to find the matching animation channel.
- A `static vector<glm::mat4> transforms` was shared across all `Model` instances — a correctness hazard when multiple models existed.
- Bone data was recomputed every time `Draw()` was called, even if the same model was drawn twice in one frame (e.g. shadow pass + main pass).

---

### Pass 1 — Channel lookup map + binary search keyframes → ~160 µs

**Changes (`model.cpp`, `model.h`):**

Added a per-animation `unordered_map<string, int>` built once at load time that maps node name → channel index. This replaced the O(n) linear scan over `animation->mChannels` done on every node visit.

Keyframe interpolation switched from a linear scan to `std::lower_bound` (binary search) over the sorted keyframe arrays. For a character with ~60 bones and hundreds of keyframes per channel this removed most of the per-frame work.

```
Before: O(channels) scan per node per frame
After:  O(1) map lookup + O(log k) binary search
```

---

### Pass 2 — Scratch buffer reuse → ~140 µs

Eliminated per-frame heap allocations inside `ComputeBoneTransformsForAnimation()`.

The `boneInfo` copy and the output `transforms` vector were previously re-allocated every frame. Replaced with pre-reserved instance members that are `.clear()`-ed and refilled in place, keeping the allocated capacity across frames.

---

### Pass 3 — Flat node list, zero recursion, zero string hashing → ~120 µs

**Change: `m_NodeList`**

At model load time, `InitializeAnimations()` now builds a flat `vector<NodeEntry>` from the scene node tree using a one-time BFS/DFS traversal. Each entry stores:

- `parentIndex` (into the flat list, or -1 for root)
- `localTransform` (the node's default bind-pose transform)
- `boneIndex` (index into `m_BoneInfo`, or -1 if not a skinned bone)
- Per-animation channel index (pre-looked-up per animation)

At runtime `ComputeBoneTransformsForAnimation()` iterates the flat list in order (parents guaranteed before children). No recursion, no string hashing, no tree traversal — just a tight loop over a cache-friendly array.

```
Before: recursive DFS + string lookup per node per frame
After:  flat loop, all lookups resolved at load time
```

---

### Pass 4 — Per-frame cache + generation counter → eliminates redundant work

**New members (`model.h`):**

```cpp
mutable vector<glm::mat4> m_CachedTransforms;
mutable uint64_t          m_TransformFrame = UINT64_MAX;
static  uint64_t          s_FrameGen;
static  void              BeginFrame() { ++s_FrameGen; }
```

**How it works:**

`BeginFrame()` is called once per game loop iteration (increments `s_FrameGen`).
`Draw()` checks `m_TransformFrame != s_FrameGen` before computing anything. If they match, the cached result from `PrepareAnimation()` is used directly — the bone transform loop runs zero times.

This matters when the same `Model*` is drawn more than once per frame (shadow pass + main pass). Previously both draws recomputed all bone transforms. Now only the first draw (or the async job) does the work.

---

### Pass 5 — Multithreaded bone computation → overlapped with render passes

**New API:**

```cpp
// model.h
void PrepareAnimation(float t);   // computes bones, stores in m_CachedTransforms
bool IsAnimated() const;

// render_manager.h
void prepareAllAnimations();       // collects unique animated models, launches std::async jobs
std::vector<std::future<void>> m_animFutures;
```

**`prepareAllAnimations()` (`render_manager.cpp`):**

Walks `currentScene->getGameObjects()`, deduplicates model pointers (multiple objects can share one `Model`), then launches one `std::async(launch::async, ...)` job per unique animated model. Returns immediately — the calling thread does not block.

`renderMainPass()` waits for all futures at its very start (`WaitForAnimations` Tracy zone) before issuing any skinned draw call.

**Thread safety:**

`Draw()` gates all bone data access behind `GLint location = shader.getUniformLocation("gBones")`. If the shader has no `gBones` uniform (e.g. the shadow depth shader), `location == -1` and the entire bone block is skipped. This made it safe to overlap the async jobs with the shadow pass without any locking.

---

### Pass 6 — Animated shadows with 1-frame latency (Option B)

**Problem with Pass 5:** The shadow pass ran while the async jobs were computing this frame's bone matrices. Even though it was data-race-free, the depth shader had no `gBones` uniform so shadows were always rendered in bind pose (T-pose).

**Solution:**

The frame ordering was restructured so the shadow pass runs *before* `BeginFrame()` is called:

```
Frame N:
  renderShadowPass()        ← reads frame N-1 m_CachedTransforms (animated, 1 frame old)
  Model::BeginFrame()       ← increments s_FrameGen to N
  prepareAllAnimations()    ← launches async jobs for frame N
  renderMainPass()          ← waits for jobs, uses fresh frame-N transforms
```

**Depth shader change (`shaders/depth_pre_pass.vs`):**

Added `gBones[MAX_BONES]` uniform and a standard 4-bone skinning expression:

```glsl
const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

void main()
{
    mat4 BoneTransform  = gBones[boneIDs[0]] * weights[0];
    BoneTransform      += gBones[boneIDs[1]] * weights[1];
    BoneTransform      += gBones[boneIDs[2]] * weights[2];
    BoneTransform      += gBones[boneIDs[3]] * weights[3];

    vec4 skinnedPos = BoneTransform * vec4(aPos, 1.0);
    gl_Position = lightSpaceMatrix * model * skinnedPos;
}
```

Because `m_TransformFrame` still equals the old `s_FrameGen` when the shadow pass runs, `Draw()` finds the cache valid and uploads last frame's bone matrices. The shadow follows the animation one frame behind — imperceptible at any reasonable frame rate.

**Result:** animated shadows, parallel bone computation, zero extra latency on the main pass.

---

### Summary Table

| Pass | Change | Cost |
|------|--------|------|
| Baseline | Inline per-draw, recursive tree walk, string search | ~220 µs |
| 1 | Channel map + binary search keyframes | ~160 µs |
| 2 | Scratch buffer reuse, no per-frame alloc | ~140 µs |
| 3 | Flat node list, zero recursion, zero string hash | ~120 µs |
| 4 | Per-frame cache + generation counter | redundant draws → 0 |
| 5 | `std::async` per model, overlapped with shadow pass | off critical path |
| 6 | Animated shadows (1-frame latency) via BeginFrame reorder | shadows no longer bind-pose |
