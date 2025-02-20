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

#include <filesystem>
// Required for std::filesystem
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>
namespace fs = std::filesystem; // Alias for convenience

// Game-related State data
SpriteRenderer *Renderer;
float timeSinceLastBuild;

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
    float timeSinceLastBuild = 0;
    // load shaders
    // Define base paths
    fs::path shaderBasePath = "shaders";
    fs::path textureBasePath = "assets";

    // Load shaders using filesystem paths
    ResourceManager::LoadShader((shaderBasePath / "sprite.vs").string().c_str(),
                                (shaderBasePath / "sprite.frag").string().c_str(),
                                nullptr, "sprite");

    // Configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
                                      static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);

    // Set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));

    // Load textures using filesystem paths
    ResourceManager::LoadTexture((textureBasePath / "psyduck.png").string().c_str(), true, "face");
    ResourceManager::LoadTexture((textureBasePath / "grass2.png").string().c_str(), false, "grass");
    // ResourceManager::LoadTexture((textureBasePath / "awesomeface.png").string(), true, "face2");
    // ResourceManager::LoadTexture((textureBasePath / "block.png").string(), false, "block");
    ResourceManager::LoadTexture((textureBasePath / "house.png").string().c_str(), true, "block_solid");

    // load levels
    GameLevel one;
    one.Load("levels/one.lvl", this->Width, this->Height / 2);
    GameLevel two;
    two.Load("levels/two.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Level = 0;
    mainChar = new Player(glm::vec2(100.0f, 50.0f), glm::vec2(100.0f, 100.0f), ResourceManager::GetTexture("face"), Speeds{10.0f, 10.0f, 10.0f, 10.0f});
}

void Game::Update(float dt)
{
    DoCollisions();
}

void Game::ProcessInput(float dt)
{
    // std::cout << "\n"
    //           << timeSinceLastBuild << "\n";
    timeSinceLastBuild += dt;
    if (this->Keys[GLFW_KEY_W] == true)
    {
        std::cout << "pressed W";
        mainChar->Position = mainChar->Position + mainChar->Speeds.Up * glm::vec2(0.0f, -1.0f);
    }
    if (this->Keys[GLFW_KEY_S] == true)
    {
        std::cout << "pressed S";
        mainChar->Position = mainChar->Position + mainChar->Speeds.Down * glm::vec2(0.0f, 1.0f);
    }
    if (this->Keys[GLFW_KEY_A] == true)
    {
        std::cout << "pressed A";
        mainChar->Position = mainChar->Position + mainChar->Speeds.Left * glm::vec2(-1.0f, 0.0f);
    }
    if (this->Keys[GLFW_KEY_D] == true)
    {
        std::cout << "pressed D";
        mainChar->Position = mainChar->Position + mainChar->Speeds.Right * glm::vec2(1.0f, 0.0f);
    }
    if (this->Keys[GLFW_KEY_E] == true && timeSinceLastBuild > 0.500f)
    {
        std::cout << "pressed E";
        this->Levels[this->Level].buildHouse(this->Width, this->Height, mainChar->Position);
        timeSinceLastBuild = 0;
    }
    if (this->Keys[GLFW_KEY_I] == true)
    {
        std::cout << "pressed I \n";
        std::cout << this->Levels[0].Bricks.size();
    }
}

void Game::Render()
{
    Renderer->DrawSprite(ResourceManager::GetTexture("grass"), glm::vec2(0.0f, 0.0f), glm::vec2(900.0f, 900.0f), .0f, glm::vec3(1.0f, 0.0f, 1.0f));
    this->Levels[this->Level].Draw(*Renderer);
    Renderer->DrawSprite(mainChar->Sprite, mainChar->Position, glm::vec2(100.0f, 100.0f), .0f, glm::vec3(1.0f, 1.0f, 1.0f));
    // Renderer->DrawSprite(ResourceManager::GetTexture("face2"), glm::vec2(100.0f, 50.0f), glm::vec2(100.0f, 100.0f), .0f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void Game::updateResolution(unsigned int width, unsigned int height)
{
    Width = width;
    Height = height;
}

void Game::deletePlayer()
{
    delete mainChar;
    mainChar = nullptr; // Prevents dangling pointer issues
}

void Game::DoCollisions()
{
    for (GameObject &box : this->Levels[this->Level].Bricks)
    {
        // std::cout << "first ciao";
        if (mainChar->CheckCollision(box))
        {
            return;
        }
        else
        {
            mainChar->Speeds.Up = 10.0f;
            mainChar->Speeds.Right = 10.0f;
            mainChar->Speeds.Down = 10.0f;
            mainChar->Speeds.Left = 10.0f;
        }
    }
}