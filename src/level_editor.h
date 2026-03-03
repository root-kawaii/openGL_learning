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
    void setRenderManager(RenderManager *rm) { renderManager = rm; };

private:
    RenderManager *renderManager;
    std::shared_ptr<Game> game;
    std::vector<Level> levels;
    std::vector<std::string> models;
    std::vector<std::string> textures; // flat list for legacy Textures panel

    void parseLevelsList(std::string path);
    void parseModel(std::string path);
    void parseTextures(std::string path);

    bool levelsPressed = false;

    // ── Terrain texture inspector ─────────────────────────────────────────
    // Recursive scan of assets/ for all image files
    std::vector<std::string> allTexturePaths;
    void scanAllTextures(const std::string &root);

    // Picker popup state
    int  pickerSlot    = -1; // 0=grass 1=stone 2=rock2
    int  pickerMapType = -1; // 0=diff 1=nor 2=ao 3=rough
    char pickerFilter[128]  = "";

    void renderTerrainInspector();
    void renderPBRMaterialInspector();

    // State for the PBR material picker / editor
    char pbrPickerFilter[128] = "";
    char pbrNewMatName[64]    = "new_material";
    char pbrNewAlbedo[256]    = "";
    char pbrNewNormal[256]    = "";
    char pbrNewMetallic[256]  = "";
    char pbrNewRoughness[256] = "";
    char pbrNewAO[256]        = "";
    int  pbrPlaceMatIdx       = 0; // combo index for "place new tile" section
};
