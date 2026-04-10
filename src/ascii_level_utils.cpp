#include "ascii_level_utils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::string trim(const std::string &value)
{
    size_t start = value.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    size_t end = value.find_last_not_of(" \t\r");
    return value.substr(start, end - start + 1);
}

SceneObject makeSceneObject(const std::string &id,
                            const std::string &path,
                            const glm::vec3 &position,
                            const glm::vec3 &rotation,
                            const glm::vec3 &scale,
                            const std::string &shaderName,
                            const std::string &materialName,
                            const glm::vec3 &color,
                            float collisionRadius,
                            bool gameEntity,
                            const std::string &entityTag = "")
{
    SceneObject obj;
    obj.id = id;
    obj.name = id;
    obj.path = path;
    obj.position = position;
    obj.rotation = rotation;
    obj.scale = scale;
    obj.shader_name = shaderName;
    obj.materialName = materialName;
    obj.color = color;
    obj.collisionRadius = collisionRadius;
    obj.gameEntity = gameEntity;
    obj.entityTag = entityTag;
    return obj;
}
}

AsciiLevelBuildConfig AsciiLevelUtils::defaultDungeonConfig()
{
    AsciiLevelBuildConfig config;
    config.materials["cobblestone"] = {
        "assets/cobblestone_floor_09_2k/textures/cobblestone_floor_09_diff_2k.jpg",
        "",
        "assets/cobblestone_floor_09_2k/textures/cobblestone_floor_09_arm_2k.jpg",
        "assets/cobblestone_floor_09_2k/textures/cobblestone_floor_09_arm_2k.jpg",
        "assets/cobblestone_floor_09_2k/textures/cobblestone_floor_09_ao_2k.jpg",
        "assets/cobblestone_floor_09_2k/textures/cobblestone_floor_09_disp_2k.png"};
    config.materials["rusted_iron"] = {
        "assets/rustediron-streaks2-bl/rustediron-streaks_basecolor.png",
        "assets/rustediron-streaks2-bl/rustediron-streaks_normal.png",
        "assets/rustediron-streaks2-bl/rustediron-streaks_metallic.png",
        "assets/rustediron-streaks2-bl/rustediron-streaks_roughness.png",
        "",
        ""};

    config.symbols['#'] = {
        AsciiLevelSymbolDef::Kind::Wall,
        "assets/cube.obj",
        config.wallShaderName,
        config.wallMaterialName,
        "wall",
        config.wallColor,
        glm::vec3(config.cellSize, config.wallHeight, config.cellSize),
        glm::vec3(0.0f),
        glm::vec3(0.0f),
        0.0f,
        false,
        "",
        false,
        {}};

    config.symbols['C'] = {
        AsciiLevelSymbolDef::Kind::Prop,
        "assets/stone_cube.glb",
        "pbr_textured",
        "",
        "chest",
        glm::vec3(0.72f, 0.46f, 0.22f),
        glm::vec3(1.3f),
        glm::vec3(0.0f),
        glm::vec3(0.0f, -0.35f, 0.0f),
        0.55f,
        false,
        "",
        false,
        {}};

    config.symbols['E'] = {
        AsciiLevelSymbolDef::Kind::Enemy,
        "assets/mech_drone.glb",
        "pbr_model_textured",
        "",
        "enemy",
        glm::vec3(0.88f, 0.88f, 0.92f),
        glm::vec3(1.25f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.35f, 0.0f),
        0.65f,
        true,
        "enemy",
        false,
        {}};

    Light torchLight;
    torchLight.position = glm::vec3(0.0f, 3.1f, 0.0f);
    torchLight.color = glm::vec3(1.0f, 0.78f, 0.56f);
    torchLight.intensity = 8.0f;
    config.symbols['T'] = {
        AsciiLevelSymbolDef::Kind::Prop,
        "assets/torch.glb",
        "pbr_model_textured",
        "",
        "torch",
        glm::vec3(1.0f, 0.82f, 0.60f),
        glm::vec3(1.0f),
        glm::vec3(0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        0.0f,
        false,
        "",
        true,
        torchLight};

    Light lightOnly;
    lightOnly.position = glm::vec3(0.0f, 3.4f, 0.0f);
    lightOnly.color = glm::vec3(0.95f, 0.95f, 1.0f);
    lightOnly.intensity = 7.0f;
    config.symbols['L'] = {
        AsciiLevelSymbolDef::Kind::LightOnly,
        "",
        "",
        "",
        "light",
        glm::vec3(1.0f),
        glm::vec3(1.0f),
        glm::vec3(0.0f),
        glm::vec3(0.0f),
        0.0f,
        false,
        "",
        true,
        lightOnly};

    config.symbols['B'] = {
        AsciiLevelSymbolDef::Kind::Prop,
        "assets/boulder.glb",
        "pbr_model_textured",
        "",
        "boulder",
        glm::vec3(0.82f, 0.82f, 0.82f),
        glm::vec3(1.35f),
        glm::vec3(0.0f),
        glm::vec3(0.0f, -0.45f, 0.0f),
        0.7f,
        false,
        "",
        false,
        {}};

    config.symbols['P'] = {
        AsciiLevelSymbolDef::Kind::Spawn,
        "assets/capsule.obj",
        "pbr_model_textured",
        "",
        "spawn_marker",
        glm::vec3(0.28f, 0.95f, 0.38f),
        glm::vec3(0.45f, 0.9f, 0.45f),
        glm::vec3(0.0f),
        glm::vec3(0.0f, 0.35f, 0.0f),
        0.0f,
        false,
        "",
        false,
        {}};

    return config;
}

std::vector<std::string> AsciiLevelUtils::normalizeLayoutLines(const std::string &textDocument)
{
    std::stringstream stream(textDocument);
    std::vector<std::string> lines;
    std::string line;
    size_t minIndent = std::numeric_limits<size_t>::max();

    while (std::getline(stream, line))
    {
        std::string cleaned = line;
        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());
        std::string trimmed = trim(cleaned);
        if (trimmed.empty() || trimmed.rfind("//", 0) == 0 || trimmed.rfind(";", 0) == 0)
            continue;

        size_t indent = cleaned.find_first_not_of(" \t");
        if (indent != std::string::npos)
            minIndent = std::min(minIndent, indent);
        lines.push_back(cleaned);
    }

    if (lines.empty())
        return {};

    if (minIndent == std::numeric_limits<size_t>::max())
        minIndent = 0;

    size_t width = 0;
    for (auto &rawLine : lines)
    {
        std::string normalized = rawLine.substr(std::min(minIndent, rawLine.size()));
        width = std::max(width, normalized.size());
        rawLine = normalized;
    }

    for (auto &normalized : lines)
        normalized.resize(width, ' ');

    return lines;
}

AsciiLevelBuildResult AsciiLevelUtils::buildLevelFromText(const std::string &textDocument,
                                                          const AsciiLevelBuildConfig &config)
{
    AsciiLevelBuildResult result;
    result.materials = config.materials;

    std::vector<std::string> rows = normalizeLayoutLines(textDocument);
    if (rows.empty())
        return result;

    result.height = static_cast<int>(rows.size());
    result.width = static_cast<int>(rows.front().size());

    float fullWidth = static_cast<float>(result.width) * config.cellSize;
    float fullDepth = static_cast<float>(result.height) * config.cellSize;

    result.objects.push_back(makeSceneObject(
        "ascii_floor",
        "assets/cube.obj",
        glm::vec3(0.0f, config.floorY, 0.0f),
        glm::vec3(0.0f),
        glm::vec3(fullWidth, config.floorThickness, fullDepth),
        config.floorShaderName,
        config.floorMaterialName,
        config.floorColor,
        0.0f,
        false));

    result.objects.push_back(makeSceneObject(
        "ascii_ceiling",
        "assets/cube.obj",
        glm::vec3(0.0f, config.ceilingY, 0.0f),
        glm::vec3(0.0f),
        glm::vec3(fullWidth, config.ceilingThickness, fullDepth),
        config.floorShaderName,
        config.floorMaterialName,
        config.ceilingColor,
        0.0f,
        false));

    auto toWorldPosition = [&](int row, int col) -> glm::vec3
    {
        float x = (static_cast<float>(col) + 0.5f) * config.cellSize - fullWidth * 0.5f;
        float z = (static_cast<float>(row) + 0.5f) * config.cellSize - fullDepth * 0.5f;
        return glm::vec3(x, 0.0f, z);
    };

    int wallCount = 0;
    int propCount = 0;
    int enemyCount = 0;

    for (int row = 0; row < result.height; ++row)
    {
        for (int col = 0; col < result.width; ++col)
        {
            char symbol = rows[row][col];
            auto defIt = config.symbols.find(symbol);
            if (defIt == config.symbols.end())
                continue;

            const AsciiLevelSymbolDef &def = defIt->second;
            glm::vec3 cellCenter = toWorldPosition(row, col);

            if (def.kind == AsciiLevelSymbolDef::Kind::LightOnly && def.emitLight)
            {
                Light light = def.lightTemplate;
                light.position = cellCenter + def.lightTemplate.position;
                result.lights.push_back(light);
                continue;
            }

            if (def.kind == AsciiLevelSymbolDef::Kind::Empty)
                continue;

            glm::vec3 position = cellCenter + def.positionOffset;
            if (def.kind == AsciiLevelSymbolDef::Kind::Wall)
                position.y = config.wallCenterY + def.positionOffset.y;

            std::string namePrefix = def.namePrefix.empty() ? "tile" : def.namePrefix;
            int index = propCount;
            if (def.kind == AsciiLevelSymbolDef::Kind::Wall)
                index = wallCount++;
            else if (def.kind == AsciiLevelSymbolDef::Kind::Enemy)
                index = enemyCount++;
            else
                index = propCount++;

            std::string id = namePrefix + "_" + std::to_string(row) + "_" + std::to_string(col) + "_" + std::to_string(index);
            result.objects.push_back(makeSceneObject(
                id,
                def.path,
                position,
                def.rotation,
                def.scale,
                def.shaderName,
                def.materialName,
                def.color,
                def.collisionRadius,
                def.gameEntity,
                def.entityTag));

            if (def.kind == AsciiLevelSymbolDef::Kind::Spawn)
                result.playerSpawn = position;

            if (def.emitLight)
            {
                Light light = def.lightTemplate;
                light.position = cellCenter + def.lightTemplate.position;
                result.lights.push_back(light);
            }
        }
    }

    return result;
}

nlohmann::json AsciiLevelUtils::buildJsonDocument(const AsciiLevelBuildResult &result)
{
    nlohmann::json document;

    if (!result.materials.empty())
    {
        document["materials"] = nlohmann::json::object();
        for (const auto &[name, def] : result.materials)
        {
            nlohmann::json mat;
            if (!def.albedo.empty()) mat["albedo"] = def.albedo;
            if (!def.normal.empty()) mat["normal"] = def.normal;
            if (!def.metallic.empty()) mat["metallic"] = def.metallic;
            if (!def.roughness.empty()) mat["roughness"] = def.roughness;
            if (!def.ao.empty()) mat["ao"] = def.ao;
            if (!def.displacement.empty()) mat["displacement"] = def.displacement;
            document["materials"][name] = mat;
        }
    }

    document["objects"] = nlohmann::json::array();
    for (const auto &obj : result.objects)
    {
        nlohmann::json entry;
        entry["id"] = obj.id;
        entry["path"] = obj.path;
        entry["shader_name"] = obj.shader_name;
        if (!obj.materialName.empty())
            entry["material_name"] = obj.materialName;
        if (!obj.entityTag.empty())
            entry["entity"] = obj.entityTag;
        else if (obj.gameEntity)
            entry["entity"] = obj.id;
        entry["position"] = {{"x", obj.position.x}, {"y", obj.position.y}, {"z", obj.position.z}};
        entry["rotation"] = {{"x", obj.rotation.x}, {"y", obj.rotation.y}, {"z", obj.rotation.z}};
        entry["scale"] = {{"x", obj.scale.x}, {"y", obj.scale.y}, {"z", obj.scale.z}};
        entry["color"] = {{"r", obj.color.x}, {"g", obj.color.y}, {"b", obj.color.z}};
        entry["collision_radius"] = {{"v", obj.collisionRadius}};
        document["objects"].push_back(entry);
    }

    document["lights"] = nlohmann::json::array();
    for (const auto &light : result.lights)
    {
        nlohmann::json entry;
        entry["position"] = {{"x", light.position.x}, {"y", light.position.y}, {"z", light.position.z}};
        entry["color"] = {{"r", light.color.r}, {"g", light.color.g}, {"b", light.color.b}};
        entry["intensity"] = light.intensity;
        document["lights"].push_back(entry);
    }

    document["meta"] = {
        {"format", "ascii_dungeon_v1"},
        {"width", result.width},
        {"height", result.height}};

    if (result.playerSpawn)
    {
        document["meta"]["player_spawn"] = {
            {"x", result.playerSpawn->x},
            {"y", result.playerSpawn->y},
            {"z", result.playerSpawn->z}};
    }

    return document;
}

bool AsciiLevelUtils::saveLevelFromText(const std::string &textDocument,
                                        const std::string &outputJsonPath,
                                        const AsciiLevelBuildConfig &config)
{
    AsciiLevelBuildResult result = buildLevelFromText(textDocument, config);
    if (result.width == 0 || result.height == 0)
        return false;

    std::filesystem::create_directories(std::filesystem::path(outputJsonPath).parent_path());
    std::ofstream out(outputJsonPath);
    if (!out.is_open())
        return false;

    out << buildJsonDocument(result).dump(4);
    return true;
}

bool AsciiLevelUtils::saveLevelFromTextFile(const std::string &textFilePath,
                                            const std::string &outputJsonPath,
                                            const AsciiLevelBuildConfig &config)
{
    std::ifstream in(textFilePath);
    if (!in.is_open())
        return false;

    std::stringstream buffer;
    buffer << in.rdbuf();
    return saveLevelFromText(buffer.str(), outputJsonPath, config);
}
