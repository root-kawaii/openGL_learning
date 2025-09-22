#include "level_editor.h"

namespace fs = std::filesystem;

void LevelEditor::renderImGuiEditor()
{
    if (ImGui::CollapsingHeader("Levels"))
    {
        for (const auto &entry : levels)
            if (ImGui::Button(entry.levelName.c_str()))
            {
                changeScene(entry);
            }
    }
    if (ImGui::CollapsingHeader("Models"))
    {
        for (const auto &entry : models)
            if (ImGui::Button(entry.c_str()))
            {
                game->getScene()->addGameObject(entry);
            }
    }
    if (ImGui::CollapsingHeader("Textures"))
    {
        for (const auto &entry : textures)
            if (ImGui::Button(entry.c_str()))
            {
                // renderManager->renderTexture()
            }
    }
}

LevelEditor::~LevelEditor()
{
}

LevelEditor::LevelEditor()
{
    parseLevelsList("levels");
    parseModel("assets");
    parseTextures("assets");
}

void LevelEditor::parseLevelsList(std::string path)
{
    for (const auto &entry : fs::directory_iterator(path))
    {
        Level level;
        level.levelName = entry.path();
        levels.push_back(level);
    }
}

void LevelEditor::parseModel(std::string path)
{
    fs::path pathPath = path;
    for (const auto &entry : fs::directory_iterator(path))
    {
        std::string extension = entry.path().extension();
        if (extension == ".obj" || extension == ".glb")
        {
            models.push_back(entry.path());
        }
    }
}

void LevelEditor::parseTextures(std::string path)
{
    fs::path pathPath = path;
    for (const auto &entry : fs::directory_iterator(path))
    {
        std::string extension = entry.path().extension();
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
        {
            textures.push_back(entry.path());
        }
    }
}

void LevelEditor::changeScene(Level level)
{
    game->setLevel(level.levelName);
}