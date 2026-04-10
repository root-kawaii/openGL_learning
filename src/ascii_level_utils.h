#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "serialization_utilities.h"

struct AsciiLevelSymbolDef
{
    enum class Kind
    {
        Empty,
        Wall,
        Prop,
        Enemy,
        LightOnly,
        Spawn
    };

    Kind kind = Kind::Empty;
    std::string path;
    std::string shaderName = "pbr_textured";
    std::string materialName;
    std::string namePrefix;
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 positionOffset = glm::vec3(0.0f);
    float collisionRadius = 0.0f;
    bool gameEntity = false;
    std::string entityTag;
    bool emitLight = false;
    Light lightTemplate{};
};

struct AsciiLevelBuildConfig
{
    float cellSize = 4.0f;
    float floorY = -1.0f;
    float floorThickness = 2.0f;
    float ceilingY = 7.0f;
    float ceilingThickness = 2.0f;
    float wallCenterY = 2.5f;
    float wallHeight = 5.0f;
    std::string floorMaterialName = "cobblestone";
    std::string wallMaterialName = "rusted_iron";
    std::string floorShaderName = "pbr_textured";
    std::string wallShaderName = "pbr_textured";
    glm::vec3 floorColor = glm::vec3(0.65f);
    glm::vec3 ceilingColor = glm::vec3(0.28f, 0.28f, 0.30f);
    glm::vec3 wallColor = glm::vec3(0.66f, 0.68f, 0.70f);
    std::unordered_map<std::string, PBRMaterialDef> materials;
    std::unordered_map<char, AsciiLevelSymbolDef> symbols;
};

struct AsciiLevelBuildResult
{
    std::vector<SceneObject> objects;
    std::vector<Light> lights;
    std::unordered_map<std::string, PBRMaterialDef> materials;
    std::optional<glm::vec3> playerSpawn;
    int width = 0;
    int height = 0;
};

class AsciiLevelUtils
{
public:
    static AsciiLevelBuildConfig defaultDungeonConfig();
    static AsciiLevelBuildResult buildLevelFromText(const std::string &textDocument,
                                                    const AsciiLevelBuildConfig &config = defaultDungeonConfig());
    static bool saveLevelFromText(const std::string &textDocument,
                                  const std::string &outputJsonPath,
                                  const AsciiLevelBuildConfig &config = defaultDungeonConfig());
    static bool saveLevelFromTextFile(const std::string &textFilePath,
                                      const std::string &outputJsonPath,
                                      const AsciiLevelBuildConfig &config = defaultDungeonConfig());

private:
    static std::vector<std::string> normalizeLayoutLines(const std::string &textDocument);
    static nlohmann::json buildJsonDocument(const AsciiLevelBuildResult &result);
};
