/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "game_level.h"
#include <iostream>

// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>

// Game-related State data
SpriteRenderer *Renderer;

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{
}

Game::~Game()
{
    delete Renderer;
}

void Game::Init()
{
    glm::vec2 mainCharPosition = glm::vec2(100.0f, 50.0f);
    speed = 3.0f;
    // load shaders
    ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.frag", nullptr, "sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
                                      static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
    // load textures
    ResourceManager::LoadTexture("assets/psyduck.png", true, "face");
    ResourceManager::LoadTexture("assets/grass2.png", false, "grass");
    // ResourceManager::LoadTexture("assets/awesomeface.png", true, "face2");
    // ResourceManager::LoadTexture("assets/block.png", false, "block");
    ResourceManager::LoadTexture("assets/house.png", true, "block_solid");

    // load levels
    GameLevel one;
    one.Load("levels/one.lvl", this->Width, this->Height / 2);
    GameLevel two;
    two.Load("levels/two.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Level = 0;
}

void Game::Update(float dt)
{
    if (this->Keys[GLFW_KEY_W] == true)
    {
        mainCharPosition = mainCharPosition + speed * glm::vec2(0.0f, -1.0f);
    }
    if (this->Keys[GLFW_KEY_S] == true)
    {
        mainCharPosition = mainCharPosition + speed * glm::vec2(0.0f, 1.0f);
    }
    if (this->Keys[GLFW_KEY_A] == true)
    {
        mainCharPosition = mainCharPosition + speed * glm::vec2(-1.0f, 0.0f);
    }
    if (this->Keys[GLFW_KEY_D] == true)
    {
        mainCharPosition = mainCharPosition + speed * glm::vec2(1.0f, 0.0f);
    }
    if (this->Keys[GLFW_KEY_E] == true)
    {
        std::cout << "pressed E";
        this->Levels[this->Level].buildHouse(this->Width, this->Height, mainCharPosition);
    }
}

void Game::ProcessInput(float dt)
{
}

void Game::Render()
{
    Renderer->DrawSprite(ResourceManager::GetTexture("grass"), glm::vec2(0.0f, 0.0f), glm::vec2(900.0f, 900.0f), .0f, glm::vec3(1.0f, 0.0f, 1.0f));
    this->Levels[this->Level].Draw(*Renderer);
    Renderer->DrawSprite(ResourceManager::GetTexture("face"), mainCharPosition, glm::vec2(100.0f, 100.0f), .0f, glm::vec3(1.0f, 1.0f, 1.0f));
    // Renderer->DrawSprite(ResourceManager::GetTexture("face2"), glm::vec2(100.0f, 50.0f), glm::vec2(100.0f, 100.0f), .0f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void Game::updateResolution(unsigned int width, unsigned int height)
{
    Width = width;
    Height = height;
}