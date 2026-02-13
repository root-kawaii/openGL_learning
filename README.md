## Psyducking

Refactor everything to order things

Understand text rendering and display stuff I need






------------------------------------------------

NEXT TO DO:


Plan: Add EntityClass stats system to GameEntity
Context
Game entities currently have hardcoded values for speed (5), ball flight duration (1.5s/0.8s), and no concept of strength, accuracy, weight, or height. We need a class/stats system so entities can be created with different archetypes that influence gameplay mechanics.

Stats
movementSpeed (int) — tiles per turn the entity can move (replaces hardcoded speed = 5)
strength (int) — determines pass and shoot range
accuracy (int) — future use (shot precision, etc.)
weight (float) — physical mass, future use (collisions, knockback)
height (float) — entity height, future use (headers, blocking)
Changes
1. New file: src/entity_class.h
Define an enum for class types and a struct with stats:


enum class EntityClassType {
    DEFAULT,
    STRIKER,
    DEFENDER,
    MIDFIELDER,
    GOALKEEPER
};

struct EntityClass {
    EntityClassType type = EntityClassType::DEFAULT;
    int movementSpeed = 5;
    int strength = 5;
    int accuracy = 5;
    float weight = 75.0f;
    float height = 1.80f;

    static EntityClass fromType(EntityClassType type);
};
fromType() is a factory that returns preset stat blocks per enum value:

DEFAULT — balanced (5/5/5, 75kg, 1.80m)
STRIKER — high strength/accuracy, lower speed (4/8/7, 80kg, 1.82m)
DEFENDER — high speed/weight, lower accuracy (6/4/3, 85kg, 1.85m)
MIDFIELDER — high speed/accuracy, balanced (7/5/6, 70kg, 1.75m)
GOALKEEPER — high accuracy/height, lower speed (3/6/8, 90kg, 1.90m)
2. src/game_entity.h — Add EntityClass member
#include "entity_class.h"
Add EntityClass entityClass; member
Add constructor overload: GameEntity(std::string entityName, std::shared_ptr<GameObject> gameObject, EntityClass entityClass)
Replace float speed = 5 with usage of entityClass.movementSpeed
3. src/game_entity.cpp — Wire stats into gameplay
New constructor stores the EntityClass
isReachable(): use entityClass.movementSpeed instead of speed
executeQueuedMovement(): use entityClass.movementSpeed for movement speed
shootBall(): scale ballFlightDuration based on entityClass.strength (higher strength = faster/longer shots)
passBall(): scale pass duration based on entityClass.strength
4. src/scene.cpp — Assign classes during entity creation
In the entity creation block, assign an EntityClassType based on entity name:
capsule → EntityClassType::STRIKER
capsule2 → EntityClassType::DEFENDER
Others → EntityClassType::DEFAULT
Use EntityClass::fromType() to get the stat block
Can be extended to JSON later
Files modified
src/entity_class.h (new)
src/game_entity.h
src/game_entity.cpp
src/scene.cpp
Verification
Build and run, no regressions
capsule and capsule2 should have different movement ranges (visible via blue tile highlights in Move mode)
Shoot/pass durations should differ based on strength stat
Entities without a specified class use default stats




- need a playable version asap so in order:
- - add ball interaction and pass and shoot maybe
- - add actions such as push or jump
- - add actions such block
- - add action to build block below you
- - add keys support to make actions quicker
- - test with more vertical arenas
- - flickering for ghost position kinda ugly
- - add arrows for movement
- - improve serialization clarity / performance



- instancing of cubes and tiles

- cutescene try by having a for on camera position that changes and goes on
- pos for x sec
- pos for y sec
.....

- recreate this scene https://www.joosteggermont.nl/projects/project_moss/index.html

- apply textures to objects from menu + add render texture to renderManager

- imgui editors like sudo love me baby where i can select block and select texture etc...
- create nice geometry of the level, all blocks in the right place


- hardcoded res in UI projection matrix

- add features to imgui to help me add stuff to the world in a easier way
(ex buttonws with various models and various shaders etc)
- improved old stuff for water


- try to build a beautiful scene - need ImGuizmo (?)
- Game mode where i can only move horizontally and start on the plane at the right height - wip
- instancing - done for lines
- Partciles (gun smoke ?) - later
- get back at profiling
- UI and Text - later
- Implement a jump - wip
- improve shadows
- use same texture for mesh and model


 
 ?????

There are multiple techniques for this, but here's a basic setup:

    Populate the G buffer

    For each light:

    a. Check for cached depth buffer cubemaps. Render and cache if none exist.

    b. Render bounding volume of light into light accumulation buffer, using G buffer data and shadowmap.

    Full-scene color/gamma correction, post effects, using light accumulation buffer and G buffer.

If your lights are not constantly moving, and you are okay with low shadowmap resolution, this should work alright for you. Here's a good overview of deferred rendering:

http://ogldev.atspace.co.uk/www/tutorial35/tutorial35.html

And here's an example of how shadowmap caching was done in Doom: http://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
 
 ?????
