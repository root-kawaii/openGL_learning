#include "level_editor.h"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

void LevelEditor::renderImGuiEditor()
{
    renderTerrainInspector();

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

// ── Terrain texture inspector ─────────────────────────────────────────────────

void LevelEditor::scanAllTextures(const std::string &root)
{
    allTexturePaths.clear();
    for (const auto &entry : fs::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            allTexturePaths.push_back(entry.path().string());
    }
    std::sort(allTexturePaths.begin(), allTexturePaths.end());
}

void LevelEditor::renderTerrainInspector()
{
    if (!game || !game->getScene()) return;
    auto obj = game->getScene()->getSelectedGameObject();
    if (!obj || obj->shaderName != "terrain_tile_shader") return;

    if (!ImGui::CollapsingHeader("Terrain Textures", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (allTexturePaths.empty())
        scanAllTextures("assets");

    static const char *slotNames[]   = { "Grass", "Stone", "Rock2" };
    static const char *mapNames[]    = { "Diffuse", "Normal", "AO", "Roughness" };

    ImGui::Text("Terrain type: %d", obj->terrainType);
    ImGui::Separator();

    for (int slot = 0; slot < 3; ++slot)
    {
        ImGui::Text("%s", slotNames[slot]);
        ImGui::Indent(8.0f);
        for (int map = 0; map < 4; ++map)
        {
            std::string curPath = renderManager->getTerrainSlotPath(slot, map);
            // Show just the filename for brevity
            std::string label = curPath.empty() ? "(none)"
                              : fs::path(curPath).filename().string();

            // Button label unique per cell
            char btnId[64];
            snprintf(btnId, sizeof(btnId), "%s##s%dm%d", label.c_str(), slot, map);

            ImGui::Text("%s:", mapNames[map]);
            ImGui::SameLine();
            if (ImGui::Button(btnId))
            {
                pickerSlot    = slot;
                pickerMapType = map;
                pickerFilter[0] = '\0';
                ImGui::OpenPopup("##texPicker");
            }
        }
        ImGui::Unindent(8.0f);
        ImGui::Separator();
    }

    // ── Picker popup ─────────────────────────────────────────────────────────
    if (ImGui::BeginPopup("##texPicker"))
    {
        ImGui::Text("Pick texture for %s / %s",
                    slotNames[pickerSlot], mapNames[pickerMapType]);
        ImGui::Separator();
        ImGui::InputText("Filter", pickerFilter, sizeof(pickerFilter));
        ImGui::Separator();

        ImGui::BeginChild("##texList", ImVec2(420, 300), true);
        for (const auto &p : allTexturePaths)
        {
            // Apply filter (case-insensitive substring)
            if (pickerFilter[0] != '\0')
            {
                std::string lower = p;
                std::string filt  = pickerFilter;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::transform(filt.begin(),  filt.end(),  filt.begin(),  ::tolower);
                if (lower.find(filt) == std::string::npos) continue;
            }

            std::string fname = fs::path(p).filename().string();
            if (ImGui::Selectable(fname.c_str()))
            {
                renderManager->reloadTerrainSlot(pickerSlot, pickerMapType, p);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", p.c_str());
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}