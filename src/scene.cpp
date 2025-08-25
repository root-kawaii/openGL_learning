#include "scene.h"

Scene::Scene()
{
    entityCounter = 0;
    serializer.loadScene("levels/one.json");
    for (auto i : serializer.getObjects())
    {
        addGameObject(std::make_shared<GameObject>(i.id, i.path, i.position, i.rotation, i.scale, i.collisionRadius));
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

void Scene::destroyGameObject(GameObject *obj)
{
    obj->~GameObject();
}

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

uint32_t Scene::generateUniqueId()
{
    return ++entityCounter;
}

void Scene::handleInput(const glm::mat4 &view, const glm::mat4 &projection)
{
    ImGuiIO &io = ImGui::GetIO();

    // Only handle clicks if not over ImGui or ImGuizmo
    if (!io.WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            selectedObject = picker.PickObject(
                io.MousePos.x, io.MousePos.y,
                gameObjects,
                view, projection,
                io.DisplaySize.x, io.DisplaySize.y);
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
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        operation,
        mode,
        glm::value_ptr(transform));

    // Update object if gizmo was used
    if (ImGuizmo::IsUsing())
    {
        selectedObject->SetTransform(transform);
    }
}
