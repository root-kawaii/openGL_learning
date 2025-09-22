#pragma once
#include "render_manager.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <string>
#include <iostream>
#include <filesystem>
#include "scene.h"
#include "game.h"
#include "game_object.h"

class Level
{
public:
    std::string levelName;
};

class LevelEditor
{
public:
    LevelEditor();
    ~LevelEditor();
    std::string selectedLevel;
    void renderImGuiEditor();

    void changeScene(Level level);

    void setGame(std::shared_ptr<Game> gamePtr)
    {
        if (!gamePtr)
        {
            std::cerr << "Warning: Setting null game pointer in LevelEditor!" << std::endl;
            return;
        }

        game = gamePtr;
        std::cout << "LevelEditor: Game set successfully, use_count: " << game.use_count() << std::endl;
    }
    void setRenderManager(RenderManager *renderManager) { renderManager = renderManager; };

private:
    RenderManager *renderManager;
    std::shared_ptr<Game> game;
    std::vector<Level> levels;
    std::vector<std::string> models;
    std::vector<std::string> textures;

    void parseLevelsList(std::string path);
    void parseModel(std::string path);
    void parseTextures(std::string path);

    bool levelsPressed = false;
};
