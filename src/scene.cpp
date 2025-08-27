#include "scene.h"
#include "game_object.h"
#include <memory>

Scene::Scene()
{
  entityCounter = 0;
  serializer.loadScene("levels/one.json");
  for (auto i : serializer.getObjects())
  {
    addGameObject(std::make_shared<GameObject>(
        i.id, i.path, i.position, i.rotation, i.scale, i.collisionRadius));
  }
}

Scene::~Scene()
{

  serializer.saveScene("levels/one.json", gameObjects);
  // TBD
}

uint32_t Scene::addGameObject(std::shared_ptr<GameObject> gameObject)
{
  // Better ID generation
  uint32_t id = generateUniqueId();
  gameObject->ID = id;

  // Add to both containers
  objectsById[id] = gameObject;
  gameObjects.push_back(gameObject);

  return id;
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

uint32_t Scene::generateUniqueId() { return ++entityCounter; }

void Scene::handleInput(const glm::mat4 &view, const glm::mat4 &projection)
{
  ImGuiIO &io = ImGui::GetIO();

  // Only handle clicks if not over ImGui or ImGuizmo
  if (!io.WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
  {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
      selectedObject =
          picker.PickObject(io.MousePos.x, io.MousePos.y, gameObjects, view,
                            projection, io.DisplaySize.x, io.DisplaySize.y);

      if (selectedObject == nullptr)
      {
        std::cout << "grounded" << std::endl;
        groundSelection = picker.PickGroundPosition(
            io.MousePos.x, io.MousePos.y, view, projection, io.DisplaySize.x,
            io.DisplaySize.y, 0.0f);
        std::cout << groundSelection.x << std::endl;
        std::cout << groundSelection.y << std::endl;
        std::cout << groundSelection.z << std::endl;
      }
    }
  }
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

  // Get transform matrix
  glm::mat4 transform = selectedObject->GetTransform();

  // Manipulate with ImGuizmo
  ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                       operation, mode, glm::value_ptr(transform));

  // Update object if gizmo was used
  if (ImGuizmo::IsUsing())
  {
    selectedObject->SetTransform(transform);
  }
}

void Scene::addCubeOnTop()
{
  // Helper lambda to round to nearest half integer (0.5 or 1.5)
  auto roundToHalf = [](float value) -> float
  {
    float base = std::floor(value);
    return (value - base < 0.5f) ? base + 0.5f : base + 1.5f;
  };

  // Round groundSelection to nearest half integers
  glm::vec3 roundedGroundSelection(
      roundToHalf(groundSelection.x),
      roundToHalf(groundSelection.y),
      roundToHalf(groundSelection.z));

  if (!selectedObject)
  {
    std::cout << "building flat" << std::endl;
    std::shared_ptr<GameObject> p = std::make_shared<GameObject>(
        "cube", "assets/cube.obj", roundedGroundSelection,
        glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), 0.0f);
    addGameObject(p);
    selectedObject = p;

    std::cout << roundedGroundSelection.x << std::endl;
    std::cout << roundedGroundSelection.y << std::endl;
    std::cout << roundedGroundSelection.z << std::endl;
  }
  else
  {

    std::cout << "building on top" << std::endl;
    std::shared_ptr<GameObject> pp = std::make_shared<GameObject>(
        "cube", "assets/cube.obj", selectedObject->position + glm::vec3(0, 1, 0),
        glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), 0.0f);
    addGameObject(pp);
    selectedObject = pp;
  }
}
