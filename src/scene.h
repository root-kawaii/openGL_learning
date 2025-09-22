#pragma once

#include "game_object.h"
#include "render_manager.h"
#include "object_picker.h"
#include "serialization_utilities.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo/ImGuizmo.h>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <unordered_map>

class Scene
{
private:
  std::string name;
  std::string currentLevel;
  std::vector<std::shared_ptr<GameObject>> gameObjects;
  std::unordered_map<uint32_t, std::shared_ptr<GameObject>> objectsById;
  GameObject *rootObject;

  uint32_t generateUniqueId();

  SerializationUtilities serializer;
  // Scene environment data
  // Skybox skybox;
  // Environment environment;
  glm::vec3 groundSelection;
  std::shared_ptr<GameObject> selectedObject;
  ObjectPicker picker;

  RenderManager *renderManager;

  void validateAllIDs();
  void debugPrintAllObjects();

public:
  Scene();
  Scene(std::string level);
  ~Scene();
  uint32_t entityCounter = 1;
  // Pure data operations
  void addGameObject(std::string gameObjectPath);
  uint32_t addGameObject(std::shared_ptr<GameObject> gameObject);
  void destroyGameObject(GameObject *obj);
  std::shared_ptr<GameObject> findObjectByName(const std::string &name);
  std::shared_ptr<GameObject> findObjectById(uint32_t id);

  // Data access
  std::vector<std::shared_ptr<GameObject>> getGameObjects()
  {
    return gameObjects;
  };
  // Environment& getEnvironment() { return environment; }

  void handleInput(const glm::mat4 &view, const glm::mat4 &projection, RenderManager renderManager);
  void renderGizmo(const glm::mat4 &view, const glm::mat4 &projection);

  // Serialization (data persistence)
  void save(const std::string &path);
  void load(const std::string &path);

  void addCubeOnTop(std::string shaderName);
  void setSelectedObject(std::shared_ptr<GameObject> object)
  {
    selectedObject = object;
  };

  void setRenderManager(RenderManager *renderManager)
  {
    renderManager = renderManager;
  }
};
