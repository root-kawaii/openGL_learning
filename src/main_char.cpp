
/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "texture.h"
#include "sprite_renderer.h"
#include "game_object.h"
#include "main_char.h"

#include "main_char.h"

Player::Player(glm::vec2 pos, glm::vec2 size, Texture2D sprite, struct Speeds speed)
    : GameObject(pos, size, sprite), Speeds(speed) {}

void Player::Move(float dx, float dy)
{
    Position.x += dx * Speeds.Right;
    Position.y += dy * Speeds.Up;
    Position.x -= dx * Speeds.Left;
    Position.y -= dy * Speeds.Down;
}

bool Player::CheckCollision(GameObject &two) // AABB - AABB collision
{
    // collision x-axis?
    bool collisionX = this->Position.x + this->Size.x >= two.Position.x &&
                      two.Position.x + two.Size.x >= this->Position.x;

    bool collisionY = this->Position.y + this->Size.y >= two.Position.y &&
                      two.Position.y + two.Size.y >= this->Position.y;

    if (collisionX && collisionY)
    {
        float centerThisX = this->Position.x + this->Size.x / 2.0f;
        float centerTwoX = two.Position.x + two.Size.x / 2.0f;

        float centerThisY = this->Position.y + this->Size.y / 2.0f;
        float centerTwoY = two.Position.y + two.Size.y / 2.0f;

        float deltaX = centerThisX - centerTwoX;
        float deltaY = centerThisY - centerTwoY;

        float overlapX = (this->Size.x / 2.0f) + (two.Size.x / 2.0f) - std::abs(deltaX);
        float overlapY = (this->Size.y / 2.0f) + (two.Size.y / 2.0f) - std::abs(deltaY);

        if (overlapX < overlapY)
        {
            if (deltaX < 0)
                this->Speeds.Right = 0;
            else
                this->Speeds.Left = 0;
        }
        else
        {
            if (deltaY < 0)
                this->Speeds.Down = 0;
            else
                this->Speeds.Up = 0;
        }
    }

    return collisionX && collisionY;
}