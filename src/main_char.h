#ifndef MAIN_CHAR_H
#define MAIN_CHAR_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "texture.h"
#include "sprite_renderer.h"
#include "game_object.h"

struct Speeds
{
    float Up;
    float Down;
    float Right;
    float Left;
};

class Player : public GameObject
{
public:
    Speeds Speeds;

    // Constructor
    Player(glm::vec2 pos, glm::vec2 size, Texture2D sprite, struct Speeds speeds);

    // Movement function
    void Move(float dx, float dy);

    bool CheckCollision(GameObject &two);
};

#endif // MAIN_CHAR_H
