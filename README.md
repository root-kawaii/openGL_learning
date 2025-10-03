## Psyducking

Refactor everything to order things

Understand text rendering and display stuff I need






------------------------------------------------

NEXT TO DO:

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
