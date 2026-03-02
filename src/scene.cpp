#include "scene.h"
#include "game_object.h"
#include "game.h"
#include "ui.h"
#include <memory>
#include <future>
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

void discretizePosition(glm::vec3 &position)
{
  position.x = std::floor(position.x) + 0.5f;
  position.y = std::floor(position.y) + 0.5f;
  position.z = std::floor(position.z) + 0.5f;
}

void Scene::destroyGameObject(GameObject *obj) { obj->~GameObject(); }

// linear lookup time, not made for frequent use
std::shared_ptr<GameObject> Scene::findObjectByName(const std::string &name)
{
  for (const auto &gameObject : gameObjects)
  {
    if (gameObject->name == name)
    {
      return gameObject;
    }
  }
  return nullptr; // Not found
}

// constant lookup
std::shared_ptr<GameObject> Scene::findObjectById(uint32_t id)
{
  auto it = objectsById.find(id);
  return (it != objectsById.end()) ? it->second : nullptr;
}

void Scene::renderGizmo(const glm::mat4 &view, const glm::mat4 &projection)
{
  if (!selectedObject)
    return;

  ImGuizmo::BeginFrame();
  ImGuiIO &io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  static ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
  static ImGuizmo::MODE mode = ImGuizmo::WORLD;

  // UI for gizmo controls
  if (ImGui::Begin("Transform"))
  {
    if (ImGui::RadioButton("Translate", operation == ImGuizmo::TRANSLATE))
      operation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", operation == ImGuizmo::ROTATE))
      operation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", operation == ImGuizmo::SCALE))
      operation = ImGuizmo::SCALE;
  }
  ImGui::End();

  glm::vec3 pos = selectedObject->position;
  glm::vec3 rot = glm::degrees(selectedObject->rotation); // Convert to degrees
  glm::vec3 scl = selectedObject->scale;

  // Let ImGuizmo build the matrix
  glm::mat4 transform;
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(pos),
                                          glm::value_ptr(rot),
                                          glm::value_ptr(scl),
                                          glm::value_ptr(transform));

  // Manipulate with ImGuizmo
  ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                       operation, mode, glm::value_ptr(transform));

  // Capture state when drag starts
  bool isUsingNow = ImGuizmo::IsUsing();
  if (isUsingNow && !gizmoWasUsing)
    gizmoBefore = captureState(selectedObject);

  // Update object if gizmo was used
  if (isUsingNow)
  {
    float translation[3], rotation[3], scale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform),
                                          translation, rotation, scale);

    selectedObject->SetPosition(glm::vec3(translation[0], translation[1], translation[2]));
    // Check if the name starts with "Light_"
    if (selectedObject->name.rfind("Light_", 0) == 0)
    {
      try
      {
        // Extract the substring after "Light_"
        std::string indexStr = selectedObject->name.substr(6);
        int lightIndex = std::stoi(indexStr) - 1;

        // Ensure the index is within the bounds of your lights vector
        if (lightIndex >= 0 && lightIndex < renderManager->getSceneLights().size())
        {
          glm::vec3 newPos = glm::vec3(translation[0], translation[1], translation[2]);

          // Update the specific light in the vector
          renderManager->getSceneLights()[lightIndex].position = newPos;

          // Also update the GameObject's internal transform so the visual mesh moves
          selectedObject->SetPosition(newPos);
        }
      }
      catch (const std::exception &e)
      {
        // Handle cases where the name is "Light_abc" (not a number)
        std::cout << "Invalid light name format: " << selectedObject->name << std::endl;
      }
    }
    selectedObject->SetRotation(glm::vec3(glm::radians(rotation[0]),
                                          glm::radians(rotation[1]),
                                          glm::radians(rotation[2])));
    selectedObject->SetScale(glm::vec3(scale[0], scale[1], scale[2]));
  }

  // Push history command when drag finishes
  if (!isUsingNow && gizmoWasUsing && selectedObject)
    pushTransformCommand(selectedObject->ID, gizmoBefore, captureState(selectedObject));

  gizmoWasUsing = isUsingNow;
}

// ── Shared parallel-loading core ─────────────────────────────────────────────
void Scene::buildFromSerializer(const std::string &levelFile, bool setStartupState)
{
  serializer.loadScene(levelFile);
  entityCounter = 1;

  sceneLights = serializer.getLights();
  std::cout << "Scene: " << sceneLights.size() << " lights, loading objects..." << std::endl;

  // Step 1 — fire off one ReadFile thread per unique model path (all in parallel)
  std::unordered_map<std::string, std::future<std::shared_ptr<Assimp::Importer>>> importFutures;

  auto enqueueIfNew = [&](const std::string &path) {
    if (importFutures.find(path) == importFutures.end()) {
      importFutures[path] = std::async(std::launch::async, [path]() {
        auto imp = std::make_shared<Assimp::Importer>();
        imp->ReadFile(path,
            aiProcess_Triangulate | aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs    | aiProcess_CalcTangentSpace);
        return imp;
      });
    }
  };

  enqueueIfNew("assets/capsule.obj"); // used for light visualisers
  for (const auto &obj : serializer.getObjects())
    enqueueIfNew(obj.path);

  // Step 2 — collect results; processNode + GPU upload happen here on main thread
  std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
  for (auto &[path, fut] : importFutures)
    modelCache[path] = std::make_shared<Model>(fut.get(), path);

  std::cout << "Scene: " << modelCache.size() << " unique models loaded." << std::endl;

  // Step 3 — light visualiser capsules (share the same cached model)
  for (const auto &light : sceneLights)
  {
    auto go = std::make_shared<GameObject>(
        "Light_" + std::to_string(entityCounter),
        modelCache.at("assets/capsule.obj"),
        light.position, glm::vec3(0), glm::vec3(0.5f),
        0, "simple_color_shader", light.color);
    addGameObject(go);
  }

  if (renderManager)
    renderManager->setLights(sceneLights);

  // Step 4 — scene objects, all sharing cached models
  for (const auto &i : serializer.getObjects())
  {
    auto go = std::make_shared<GameObject>(
        i.id, modelCache.at(i.path),
        i.position, i.rotation, i.scale,
        i.collisionRadius, i.shader_name, i.color);
    go->terrainType = i.terrainType;
    go->modelPath = i.path; // preserve path so saveScene writes it correctly

    if (i.gameEntity)
    {
      EntityClassType classType = EntityClassType::DEFAULT;
      if (go->name == "capsule")       classType = EntityClassType::STRIKER;
      else if (go->name == "capsule2") classType = EntityClassType::DEFENDER;

      auto ge = std::make_shared<GameEntity>(std::to_string(entityCounter), go, classType);
      discretizePosition(ge->object->position);
      ge->setScene(this);

      if (setStartupState && go->name == "capsule")
        ge->setHasBall(true);

      gameEntities.push_back(ge);
    }

    if (go->name == "ball")
      ball = go.get();

    addGameObject(go);
  }

  std::cout << "Scene loaded: " << gameObjects.size() << " objects." << std::endl;
  validateAllIDs();
  currentLevel = levelFile;
}

// ── Three constructors all delegate to buildFromSerializer ────────────────────

Scene::Scene()
{
  buildFromSerializer("levels/two.json", false);
}

Scene::Scene(RenderManager *renderMgr)
{
  renderManager = renderMgr;
  buildFromSerializer("levels/two.json", true);
}

Scene::Scene(std::string level)
{
  buildFromSerializer(level, false);
}

uint32_t Scene::addGameObject(std::shared_ptr<GameObject> gameObject)
{
  // Generate fresh ID - always unique, always > 0
  uint32_t id = generateUniqueId();

  // Assign the generated ID to the object
  gameObject->ID = id;

  // Add to both containers
  objectsById[id] = gameObject;
  gameObjects.push_back(gameObject);

  std::cout << "Added GameObject '" << gameObject->name << "' with ID: " << id
            << std::endl;

  return id;
}

uint32_t Scene::duplicateGameObject(uint32_t originalId)
{
  auto it = objectsById.find(originalId);
  if (it == objectsById.end())
  {
    std::cerr << "Cannot duplicate: GameObject with ID " << originalId
              << " not found" << std::endl;
    return 0; // Invalid ID
  }

  // Get the original object
  std::shared_ptr<GameObject> original = it->second;

  // Create a new copy (deep copy via constructor or clone)
  auto duplicate = std::make_shared<GameObject>(*original);

  // Generate new ID for duplicate
  uint32_t newId = generateUniqueId();
  duplicate->ID = newId;

  // Add to both containers
  objectsById[newId] = duplicate;
  gameObjects.push_back(duplicate);

  std::cout << "Duplicated GameObject '" << original->name
            << "' as '" << duplicate->name << "' with ID: " << newId
            << std::endl;

  return newId;
}

void Scene::addGameObject(std::string gameObjectPath)
{
  uint32_t id = generateUniqueId();
  auto gameObject = std::make_shared<GameObject>(
      std::to_string(id), // This becomes the name, not the ID
      gameObjectPath, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0),
      glm::vec3(1, 1, 1), 0, "default", glm::vec3(1, 1, 1));

  // FIX: Use addGameObject which will assign a fresh generated ID
  uint32_t newID = addGameObject(gameObject);
}

uint32_t Scene::generateUniqueId()
{
  return entityCounter++; // Returns 1, 2, 3, 4, 5... (never 0)
}

void Scene::validateAllIDs()
{
  std::cout << "\n=== ID VALIDATION ===" << std::endl;
  std::map<uint32_t, int> idCounts;
  bool hasErrors = false;

  for (size_t i = 0; i < gameObjects.size(); ++i)
  {
    const auto &obj = gameObjects[i];
    if (!obj)
    {
      std::cout << "❌ ERROR: Null object at index " << i << std::endl;
      hasErrors = true;
      continue;
    }

    std::cout << "[" << i << "] '" << obj->name << "' -> ID: " << obj->ID
              << std::endl;

    if (obj->ID == 0)
    {
      std::cout << "  ❌ ERROR: ID 0 is reserved for background!" << std::endl;
      hasErrors = true;
    }

    idCounts[obj->ID]++;
  }

  // Check for duplicates
  for (const auto &pair : idCounts)
  {
    if (pair.second > 1)
    {
      std::cout << "❌ ERROR: ID " << pair.first << " appears " << pair.second
                << " times!" << std::endl;
      hasErrors = true;
    }
  }

  if (!hasErrors)
  {
    std::cout << "✅ All " << gameObjects.size()
              << " objects have valid unique IDs!" << std::endl;
  }

  std::cout << "ID range: 1 to " << (entityCounter - 1) << std::endl;
  std::cout << "Next ID will be: " << entityCounter << std::endl;
  std::cout << "===================" << std::endl;
}

void Scene::handleInput(const glm::mat4 &view, const glm::mat4 &projection,
                        RenderManager &renderManager)
{
  ImGuiIO &io = ImGui::GetIO();

  // Only handle clicks if not over ImGui or ImGuizmo
  if (!io.WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
  {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
      if (gameInstance->getGameMode() == GAME)
      {
        auto entity = gameInstance->getSelectedEntity();
        if (entity)
        {
          unsigned int objID =
              renderManager.getObjectId(io.MousePos.x, io.MousePos.y);
          if (objID != 0)
          {
            // Right-clicked on an object
            std::shared_ptr<GameEntity> newClick = nullptr;
            if (gameInstance)
            {
              for (auto &entity : gameInstance->getScene()->getGameEntities())
              {
                if (entity->object->ID == objID)
                {
                  newClick = entity;
                  break;
                }
              }
            }
            std::cout << "\n=== RIGHT CLICK ===" << std::endl;

            // Handle pass target selection — buffer a PASS action
            auto uiManager = gameInstance->getUIManager();
            if (uiManager && uiManager->isPassing)
            {
              if (newClick && newClick != entity)
              {
                // Find entity with ball
                std::shared_ptr<GameEntity> ballHolder = nullptr;
                for (auto &e : gameInstance->getScene()->getGameEntities())
                {
                  if (e->getHasBall())
                  {
                    ballHolder = e;
                    break;
                  }
                }
                if (ballHolder)
                {
                  BufferedAction action;
                  action.type = ActionType::PASS;
                  action.targetEntity = newClick.get();
                  action.description = "Pass -> " + newClick->object->name;
                  ballHolder->bufferAction(action);
                }
                uiManager->passTargetEntity = newClick; // keep for trajectory preview
                uiManager->isPassing = false;
              }
              return;
            }

            // Buffer a MOVE action
            if (gameInstance && gameInstance->getTurnState() == Game::TurnState::PLANNING)
            {
              glm::vec3 targetPos;
              if (newClick)
              {
                targetPos = newClick->object->position;
              }
              else
              {
                auto it = objectsById.find(objID);
                if (it != objectsById.end())
                {
                  targetPos = it->second->position;
                }
                else
                {
                  std::cout << "Cannot find clicked object" << std::endl;
                  return;
                }
              }
              BufferedAction action;
              action.type = ActionType::MOVE;
              action.targetPosition = targetPos;
              action.description = "Move (" + std::to_string((int)targetPos.x) + ", " + std::to_string((int)targetPos.z) + ")";
              entity->bufferAction(action);
            }
            else
            {
              std::cout << "Cannot queue movement - not in planning phase" << std::endl;
            }

            std::cout << "===================" << std::endl;
          }
        }
      }
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
      // Request ID buffer update for accurate mouse picking
      renderManager.requestIDBufferUpdate();

      unsigned int objID =
          renderManager.getObjectId(io.MousePos.x, io.MousePos.y);
      std::cout << "\n=== MOUSE CLICK ===" << std::endl;
      std::cout << "Mouse position: (" << io.MousePos.x << ", " << io.MousePos.y
                << ")" << std::endl;
      std::cout << "ID buffer returned: " << objID << std::endl;

      std::shared_ptr<GameEntity> clickedEntity = nullptr;

      if (gameInstance->getGameMode() == GAME)
      {
        if (gameInstance)
        {
          for (auto &entity : gameInstance->getScene()->getGameEntities())
          {
            if (entity->object->ID == objID)
            {
              clickedEntity = entity;
              break;
            }
          }
        }
        // Only change selection if we clicked on an actual entity
        if (clickedEntity && gameInstance->isEntitySelectable(clickedEntity))
        {
          gameInstance->setSelectedEntity(clickedEntity);
        }
        // Don't deselect when clicking background or non-entity objects
        std::cout << "===================" << std::endl;
        return;
      }

      if (objID == 0)
      {
        // Background clicked
        std::cout << "✓ Background clicked (ID 0)" << std::endl;
        selectedObject = nullptr;

        // Handle ground selection
        std::cout << "Calculating ground position..." << std::endl;
        groundSelection = picker.PickGroundPosition(
            io.MousePos.x, io.MousePos.y, view, projection, io.DisplaySize.x,
            io.DisplaySize.y, 0.0f);

        std::cout << "Ground position: (" << groundSelection.x << ", "
                  << groundSelection.y << ", " << groundSelection.z << ")"
                  << std::endl;
      }
      else
      {
        // Object clicked - find it safely
        auto it = objectsById.find(objID);
        if (it != objectsById.end())
        {
          // Find if this object is a game entity
          clickedEntity = nullptr;
          if (gameInstance)
          {
            for (auto &entity : gameInstance->getScene()->getGameEntities())
            {
              if (entity->object->ID == objID)
              {
                clickedEntity = entity;
                break;
              }
            }
          }

          // Check if it's a selectable entity
          if (clickedEntity && gameInstance && gameInstance->isEntitySelectable(clickedEntity))
          {
            // Can select any selectable entity during player turn
            selectedObject = it->second;
            gameInstance->setSelectedEntity(clickedEntity);
            std::cout << "✓ Selected entity: '" << selectedObject->name << "' (ID: " << objID << ")";

            if (clickedEntity->hasMovedThisTurn)
            {
              std::cout << " [ALREADY MOVED]";
            }
            std::cout << std::endl;
          }
          else
          {
            // Not a selectable entity, just select the object
            selectedObject = it->second;
            std::cout << "✓ Selected object: '" << selectedObject->name
                      << "' (ID: " << objID << ")" << std::endl;
          }
        }
        else
        {
          std::cout << "❌ ERROR: ID " << objID << " not found in scene!"
                    << std::endl;
          std::cout << "Valid IDs in scene: ";
          for (const auto &pair : objectsById)
          {
            std::cout << pair.first << " ";
          }
          std::cout << std::endl;

          // This indicates a problem with ID buffer rendering
          std::cout << "⚠️  This suggests the ID buffer contains wrong data!"
                    << std::endl;
          selectedObject = nullptr;
        }
      }
      std::cout << "===================" << std::endl;
    }
  }
}

// Enhanced cube creation with proper ID handling
void Scene::addCubeOnTop(std::string shader_name)
{
  // Helper lambda to round to nearest half integer (0.5 or 1.5)
  auto roundToHalf = [](float value) -> float
  {
    float base = std::floor(value);
    return (value - base < 0.5f) ? base + 0.5f : base + 1.5f;
  };

  // Round groundSelection to nearest half integers
  glm::vec3 roundedGroundSelection(roundToHalf(groundSelection.x),
                                   roundToHalf(groundSelection.y),
                                   roundToHalf(groundSelection.z));

  if (!selectedObject)
  {
    std::cout << "Building cube on ground..." << std::endl;

    // Create GameObject - addGameObject will assign ID
    std::shared_ptr<GameObject> newCube = std::make_shared<GameObject>(
        "cube", "assets/cube.obj", roundedGroundSelection, glm::vec3(0, 0, 0),
        glm::vec3(1, 1, 1), 0.0f, shader_name, glm::vec3(1, 1, 1));

    uint32_t cubeID = addGameObject(newCube); // This assigns the ID
    selectedObject = newCube;

    std::cout << "Created cube at (" << roundedGroundSelection.x << ", "
              << roundedGroundSelection.y << ", " << roundedGroundSelection.z
              << ") with ID: " << cubeID << std::endl;
  }
  else
  {
    std::cout << "Building cube on top of selected object..." << std::endl;

    glm::vec3 newPosition = selectedObject->position + glm::vec3(0, 1, 0);

    std::shared_ptr<GameObject> newCube = std::make_shared<GameObject>(
        "cube", "assets/cube.obj", newPosition, glm::vec3(0, 0, 0),
        glm::vec3(1, 1, 1), 0.0f, shader_name, glm::vec3(1, 1, 1));

    uint32_t cubeID = addGameObject(newCube); // This assigns the ID
    selectedObject = newCube;

    std::cout << "Created cube at (" << newPosition.x << ", " << newPosition.y
              << ", " << newPosition.z << ") with ID: " << cubeID << std::endl;
  }
}

void Scene::addCubeBelowEntity(std::shared_ptr<GameEntity> entity, std::string shaderName)
{
  if (!entity || !entity->object)
  {
    std::cout << "No entity provided!" << std::endl;
    return;
  }

  // Get entity's current position
  glm::vec3 entityPos = entity->object->position;

  // Create cube at entity's current position (entity will move up)
  glm::vec3 cubePosition = glm::vec3(entityPos.x, entityPos.y - 0.5f, entityPos.z);

  std::shared_ptr<GameObject> newCube = std::make_shared<GameObject>(
      "cube", "assets/cube.obj", cubePosition, glm::vec3(0, 0, 0),
      glm::vec3(1, 1, 1), 0.0f, shaderName, glm::vec3(1, 1, 1));

  uint32_t cubeID = addGameObject(newCube);

  // Move entity up by 1 unit
  entity->object->position.y += 1.0f;

  std::cout << "Created cube at (" << cubePosition.x << ", " << cubePosition.y
            << ", " << cubePosition.z << ") with ID: " << cubeID
            << ". Entity moved to y=" << entity->object->position.y << std::endl;
}

void Scene::copyEntity()
{
  // Check if there's a selected object to copy
  if (!selectedObject)
  {
    std::cout << "No object selected to copy!" << std::endl;
    return;
  }

  std::cout << "Copying entity '" << selectedObject->name << "' (ID: " << selectedObject->ID << ")..." << std::endl;

  // Create a new GameObject with copied properties
  // Note: Don't pass an ID - let addGameObject() assign a fresh one
  auto copiedGameObject = std::make_shared<GameObject>(
      selectedObject->name + std::to_string(selectedObject->ID), // Give it a distinct name
      selectedObject->modelPath,                                 // Same model
      selectedObject->position + glm::vec3(1.0f, 0.0f, 0.0f),    // Offset position slightly
      selectedObject->rotation,                                  // Same rotation
      selectedObject->scale,                                     // Same scale (not hardcoded 100,100,100!)
      selectedObject->collisionRadius,                           // Same collision radius
      selectedObject->shaderName,                                // Same shader
      selectedObject->color);

  // Add the copied object to the scene - this will assign a fresh ID
  uint32_t newID = addGameObject(copiedGameObject);

  // Select the newly copied object
  selectedObject = copiedGameObject;

  std::cout << "Created copy with ID: " << newID << " at position ("
            << copiedGameObject->position.x << ", "
            << copiedGameObject->position.y << ", "
            << copiedGameObject->position.z << ")" << std::endl;
}

void Scene::copyToClipboard()
{
  if (!selectedObject)
  {
    std::cout << "Nothing selected to copy." << std::endl;
    return;
  }
  clipboardObject = selectedObject;
  std::cout << "Copied '" << selectedObject->name << "' to clipboard." << std::endl;
}

void Scene::pasteFromClipboard()
{
  if (!clipboardObject)
  {
    std::cout << "Clipboard is empty." << std::endl;
    return;
  }

  auto pasted = std::make_shared<GameObject>(
      clipboardObject->name,
      clipboardObject->modelPath,
      clipboardObject->position + glm::vec3(1.0f, 0.0f, 0.0f),
      clipboardObject->rotation,
      clipboardObject->scale,
      clipboardObject->collisionRadius,
      clipboardObject->shaderName,
      clipboardObject->color);

  addGameObject(pasted);
  selectedObject = pasted;

  // Record in history so Ctrl+Z can remove it
  HistoryCommand cmd;
  cmd.type        = HistoryCmdType::ADD_OBJECT;
  cmd.addedObject = pasted;
  history.push(cmd);

  std::cout << "Pasted '" << pasted->name << "' at ("
            << pasted->position.x << ", " << pasted->position.y
            << ", " << pasted->position.z << ")." << std::endl;
}

// ---------------------------------------------------------------------------
// History helpers
// ---------------------------------------------------------------------------
TransformState Scene::captureState(const std::shared_ptr<GameObject>& obj) const
{
  TransformState s;
  s.position = obj->position;
  s.rotation = obj->rotation;
  s.scale    = obj->scale;
  s.color    = obj->color;
  return s;
}

void Scene::applyState(const std::shared_ptr<GameObject>& obj, const TransformState& s)
{
  obj->SetPosition(s.position);
  obj->SetRotation(s.rotation);
  obj->SetScale(s.scale);
  obj->color = s.color;
}

void Scene::removeGameObjectById(uint32_t id)
{
  objectsById.erase(id);
  gameObjects.erase(
      std::remove_if(gameObjects.begin(), gameObjects.end(),
                     [id](const auto& o) { return o->ID == id; }),
      gameObjects.end());
  if (selectedObject && selectedObject->ID == id)
    selectedObject = nullptr;
}

void Scene::reInsertGameObject(std::shared_ptr<GameObject> obj)
{
  objectsById[obj->ID] = obj;
  gameObjects.push_back(obj);
  selectedObject = obj;
}

void Scene::pushTransformCommand(uint32_t id,
                                  const TransformState& before,
                                  const TransformState& after)
{
  HistoryCommand cmd;
  cmd.type     = HistoryCmdType::TRANSFORM;
  cmd.objectId = id;
  cmd.before   = before;
  cmd.after    = after;
  history.push(cmd);
}

void Scene::undo()
{
  if (!history.canUndo()) return;

  HistoryCommand cmd = history.popUndo();

  if (cmd.type == HistoryCmdType::TRANSFORM)
  {
    auto it = objectsById.find(cmd.objectId);
    if (it == objectsById.end()) return;
    // Swap: after → before on object, keep after in cmd for redo
    applyState(it->second, cmd.before);
    selectedObject = it->second;
    history.pushRedo(cmd);
  }
  else if (cmd.type == HistoryCmdType::ADD_OBJECT)
  {
    // Undo of add = remove the object
    removeGameObjectById(cmd.addedObject->ID);
    history.pushRedo(cmd);
  }
}

void Scene::redo()
{
  if (!history.canRedo()) return;

  HistoryCommand cmd = history.popRedo();

  if (cmd.type == HistoryCmdType::TRANSFORM)
  {
    auto it = objectsById.find(cmd.objectId);
    if (it == objectsById.end()) return;
    applyState(it->second, cmd.after);
    selectedObject = it->second;
    history.pushUndo(cmd);
  }
  else if (cmd.type == HistoryCmdType::ADD_OBJECT)
  {
    // Redo of add = re-insert the same object with same ID
    reInsertGameObject(cmd.addedObject);
    history.pushUndo(cmd);
  }
}

// Destructor - save scene with current state
Scene::~Scene()
{
  std::cout << "Saving scene with generated IDs..." << std::endl;
  serializer.saveScene(currentLevel, gameObjects, renderManager->getSceneLights());
}

// Debug method to print all objects and their IDs
void Scene::debugPrintAllObjects()
{
  std::cout << "\n=== ALL SCENE OBJECTS ===" << std::endl;
  std::cout << "Total objects: " << gameObjects.size() << std::endl;

  for (size_t i = 0; i < gameObjects.size(); ++i)
  {
    const auto &obj = gameObjects[i];
    if (obj)
    {
      std::cout << "[" << i << "] ID:" << obj->ID << " Name:'" << obj->name
                << "' Path:'" << obj->modelPath << "'" << std::endl;
    }
    else
    {
      std::cout << "[" << i << "] ❌ NULL OBJECT" << std::endl;
    }
  }
  std::cout << "=========================" << std::endl;
}

void Scene::renderCompactColorPicker()
{
  if (ImGui::Begin("Color Picker", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    // Main HSV wheel picker - this is the key widget that creates the circular interface
    ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_PickerHueWheel | // Creates the wheel
        ImGuiColorEditFlags_NoSidePreview |  // No side preview
        ImGuiColorEditFlags_NoSmallPreview | // No small preview
        ImGuiColorEditFlags_NoInputs |       // No built-in input fields
        ImGuiColorEditFlags_AlphaBar;        // Separate alpha bar

    ImGui::ColorPicker4("##picker", &selectedColor.r, flags);

    // Bottom info panel (like in your image)
    ImGui::Separator();

    // Display values in a compact layout
    ImGui::Columns(4, "ColorInfo", false);

    // RGB
    ImGui::Text("R: %d", (int)(selectedColor.r * 255));
    ImGui::NextColumn();
    ImGui::Text("G: %d", (int)(selectedColor.g * 255));
    ImGui::NextColumn();
    ImGui::Text("B: %d", (int)(selectedColor.b * 255));
    ImGui::NextColumn();
    ImGui::Text("A: %.2f", selectedColor.a);

    ImGui::Columns(1);

    // Hex display
    ImGui::Text("HEX: #%s", rgbToHex(glm::vec3(selectedColor)).c_str());

    if (ImGui::Button("Apply", ImVec2(-1, 0)))
    {
      TransformState before = captureState(selectedObject);
      selectedObject->color = glm::vec3(selectedColor.x, selectedColor.y, selectedColor.z);
      pushTransformCommand(selectedObject->ID, before, captureState(selectedObject));
    }
    if (ImGui::Button("Move", ImVec2(-2, 0)))
    {
      gameEntities.back().get()->isMoving = true;
      gameEntities.back()
          .get()
          ->targetDestination = glm::vec3(0, 0, 0);
    }
    if (ImGui::Button("Discretize", ImVec2(-3, 0)))
    {
      for (auto &k : gameEntities)
      {
        discretizePosition(k->object->position);
      }
    }
  }
  ImGui::End();
}

// Update occupancy map after collision
void Scene::updateOccupancyAfterCollision(glm::ivec3 cell, const std::vector<std::shared_ptr<GameEntity>> &entities)
{
  // This is a placeholder for future occupancy tracking
  // For now, just log the collision
  std::cout << "[Occupancy] Cell (" << cell.x << ", " << cell.z
            << ") now has " << entities.size() << " entities" << std::endl;
}
