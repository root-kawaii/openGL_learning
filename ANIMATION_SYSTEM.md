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
