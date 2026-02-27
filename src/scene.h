#pragma once

#include "game_object.h"
#include "render_manager.h"
#include "object_picker.h"
#include "serialization_utilities.h"
#include "history.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo/ImGuizmo.h>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <unordered_map>
#include "game_entity.h"

class RenderManager;
class Game;
class UIManager;

class Scene
{
private:
  std::string name;
  std::string currentLevel;
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::unordered_map<uint32_t, std::shared_ptr<GameObject>> objectsById;
  std::vector<std::shared_ptr<GameEntity>> gameEntities;
  std::vector<Light> sceneLights;
  GameObject *rootObject;

  glm::vec4 selectedColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Start with red
  bool showAdvancedColorPicker = false;

  uint32_t generateUniqueId();

  SerializationUtilities serializer;
  // Scene environment data
  // Skybox skybox;
  // Environment environment;
  glm::vec3 groundSelection;
  std::shared_ptr<GameObject> selectedObject;
  std::shared_ptr<GameObject> clipboardObject;   // Shift+C stores here
  ObjectPicker picker;

  // Undo / redo
  HistoryManager history;
  bool           gizmoWasUsing = false;
  TransformState gizmoBefore;

  // Helpers
  TransformState captureState(const std::shared_ptr<GameObject>& obj) const;
  void           applyState(const std::shared_ptr<GameObject>& obj,
                             const TransformState& s);
  void           removeGameObjectById(uint32_t id);
  void           reInsertGameObject(std::shared_ptr<GameObject> obj);

  RenderManager *renderManager;
  Game *gameInstance = nullptr;
  UIManager *uiManager = nullptr;

  void validateAllIDs();
  void debugPrintAllObjects();

public:
  Scene();
  Scene(RenderManager *renderMgr);
  Scene(std::string level);
  ~Scene();
  uint32_t entityCounter = 1;
  GameObject *ball;
  // Pure data operations
  void addGameObject(std::string gameObjectPath);
  uint32_t addGameObject(std::shared_ptr<GameObject> gameObject);
  uint32_t duplicateGameObject(uint32_t originalId);
  void destroyGameObject(GameObject *obj);
  std::shared_ptr<GameObject> findObjectByName(const std::string &name);
  std::shared_ptr<GameObject> findObjectById(uint32_t id);

  // Data access
  std::vector<std::shared_ptr<GameObject>> getGameObjects()
  {
    return gameObjects;
  };
  std::vector<std::shared_ptr<GameEntity>> getGameEntities()
  {
    return gameEntities;
  };
  std::vector<Light> getLights()
  {
    return sceneLights;
  };
  // Environment& getEnvironment() { return environment; }

  std::shared_ptr<GameObject> getSelectedGameObject() { return selectedObject; };

  void handleInput(const glm::mat4 &view, const glm::mat4 &projection,
                   RenderManager &renderManager);
  void renderGizmo(const glm::mat4 &view, const glm::mat4 &projection);

  // Serialization (data persistence)
  void save(const std::string &path);
  void load(const std::string &path);

  void copyEntity();
  void copyToClipboard();
  void pasteFromClipboard();

  // History
  void undo();
  void redo();
  void pushTransformCommand(uint32_t id,
                             const TransformState& before,
                             const TransformState& after);

  void renderCompactColorPicker();

  void addCubeOnTop(std::string shaderName);
  void addCubeBelowEntity(std::shared_ptr<GameEntity> entity, std::string shaderName);
  void setSelectedObject(std::shared_ptr<GameObject> object)
  {
    selectedObject = object;
  };

  void setRenderManager(RenderManager *renderManager)
  {
    renderManager = renderManager;
  }

  void setGame(Game *game) { gameInstance = game; }
  void setUIManager(UIManager *ui) { uiManager = ui; }

  // Occupancy map for entity collision tracking
  void updateOccupancyAfterCollision(glm::ivec3 cell, const std::vector<std::shared_ptr<GameEntity>> &entities);

  // Helper function to convert RGB to hex string
  std::string rgbToHex(const glm::vec3 &color)
  {
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(2) << (int)(color.r * 255)
       << std::setw(2) << (int)(color.g * 255)
       << std::setw(2) << (int)(color.b * 255);
    std::string result = ss.str();
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
  }

  // Helper function to convert hex string to RGB
  glm::vec3 hexToRgb(const std::string &hex)
  {
    std::string cleanHex = hex;
    if (cleanHex[0] == '#')
      cleanHex = cleanHex.substr(1);
    if (cleanHex.length() != 6)
      return glm::vec3(selectedColor.r, selectedColor.g, selectedColor.b);

    try
    {
      unsigned int value = std::stoul(cleanHex, nullptr, 16);
      return glm::vec3(
          ((value >> 16) & 0xFF) / 255.0f,
          ((value >> 8) & 0xFF) / 255.0f,
          (value & 0xFF) / 255.0f);
    }
    catch (...)
    {
      return glm::vec3(selectedColor.r, selectedColor.g, selectedColor.b);
    }
  }
};
