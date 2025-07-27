## Psyducking

Refactor everything to order things

Understand text rendering and display stuff I need






------------------------------------------------

NEXT TO DO:

- Implement object system
- Advance in PBR
- improve shadows


 
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