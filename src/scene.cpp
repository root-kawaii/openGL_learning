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
