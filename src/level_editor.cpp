#include "level_editor.h"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

void LevelEditor::renderImGuiEditor()
{
    renderTerrainInspector();
    renderPBRMaterialInspector();

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

// ── PBR material inspector ────────────────────────────────────────────────────
// Shown for any object whose shader is "pbr_textured".
// Lets you:
//   1. See + assign which named material the object uses.
//   2. Define new named materials (5 texture-path inputs).
//   3. Load textures into the GPU immediately via loadPBRMaterials().

void LevelEditor::renderPBRMaterialInspector()
{
    if (!game || !game->getScene()) return;
    auto obj = game->getScene()->getSelectedGameObject();
    if (!obj || obj->shaderName != "pbr_textured") return;

    if (!ImGui::CollapsingHeader("PBR Material", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    Scene *scene = game->getScene();
    const auto &mats = scene->getMaterials();

    // ── 1. Assign material to selected object ─────────────────────────────
    ImGui::Text("Assigned material:  %s",
                obj->materialName.empty() ? "(none)" : obj->materialName.c_str());

    // Build a list of known material names for the combo
    std::vector<const char *> matNames;
    matNames.push_back("(none)");
    for (const auto &[name, _] : mats)
        matNames.push_back(name.c_str());

    int current = 0; // default = (none)
    for (int i = 1; i < static_cast<int>(matNames.size()); ++i)
        if (obj->materialName == matNames[i]) { current = i; break; }

    if (ImGui::Combo("Material##assign", &current, matNames.data(),
                     static_cast<int>(matNames.size())))
    {
        obj->materialName = (current == 0) ? "" : matNames[current];
    }

    ImGui::Separator();

    // ── 2. Define a new named material ───────────────────────────────────
    ImGui::Text("Define new material:");
    ImGui::InputText("Name##pbrNew",      pbrNewMatName,  sizeof(pbrNewMatName));
    ImGui::InputText("Albedo##pbrNew",    pbrNewAlbedo,   sizeof(pbrNewAlbedo));
    ImGui::InputText("Normal##pbrNew",    pbrNewNormal,   sizeof(pbrNewNormal));
    ImGui::InputText("Metallic##pbrNew",  pbrNewMetallic, sizeof(pbrNewMetallic));
    ImGui::InputText("Roughness##pbrNew", pbrNewRoughness,sizeof(pbrNewRoughness));
    ImGui::InputText("AO##pbrNew",        pbrNewAO,       sizeof(pbrNewAO));

    if (ImGui::Button("Add material + upload textures"))
    {
        if (pbrNewMatName[0] != '\0')
        {
            PBRMaterialDef def;
            def.albedo    = pbrNewAlbedo;
            def.normal    = pbrNewNormal;
            def.metallic  = pbrNewMetallic;
            def.roughness = pbrNewRoughness;
            def.ao        = pbrNewAO;

            scene->setMaterial(pbrNewMatName, def);

            // Immediately upload to GPU so objects using it render correctly
            if (renderManager)
            {
                std::unordered_map<std::string, PBRMaterialDef> single;
                single[pbrNewMatName] = def;
                renderManager->loadPBRMaterials(single);
            }
        }
    }
}