#include <iostream>
#include <filesystem>
#include "game_object.h"
#include <unordered_map>
#include "serialization_utilities.h"

class Scene
{
private:
    std::string name;
    std::vector<std::shared_ptr<GameObject>> gameObjects;
    std::unordered_map<uint32_t, std::shared_ptr<GameObject>> objectsById;
    GameObject *rootObject;

    uint32_t generateUniqueId();

    SerializationUtilities serializer;
    // Scene environment data
    // Skybox skybox;
    // Environment environment;

public:
    Scene();
    ~Scene();
    uint64_t entityCounter;
    // Pure data operations
    uint32_t addGameObject(std::shared_ptr<GameObject> gameObject);
    void destroyGameObject(GameObject *obj);
    std::shared_ptr<GameObject> findObjectByName(const std::string &name);
    std::shared_ptr<GameObject> findObjectById(uint32_t id);

    // Data access
    std::vector<std::shared_ptr<GameObject>> getGameObjects() { return gameObjects; };
    // Environment& getEnvironment() { return environment; }

    // Serialization (data persistence)
    void save(const std::string &path);
    void load(const std::string &path);
};