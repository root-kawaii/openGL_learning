#include <iostream>
#include <filesystem>
#include "game_object.h"
#include <unordered_map>


class Scene {
private:
    std::string name;
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::unordered_map<uint32_t, GameObject*> objectsById;
    GameObject* rootObject;

    uint32_t generateUniqueId();
    
    // Scene environment data
    // Skybox skybox;
    // Environment environment;
    
public:
    Scene();
    ~Scene();
    uint64_t entityCounter;
    // Pure data operations
    uint32_t addGameObject(std::unique_ptr<GameObject> gameObject);
    void destroyGameObject(GameObject* obj);
    GameObject* findObjectByName(const std::string& name);
    GameObject* findObjectById(uint32_t id);
    
    // Data access
    const std::vector<std::unique_ptr<GameObject>>& getGameObjects() const;
    // Environment& getEnvironment() { return environment; }
    
    // Serialization (data persistence)
    void save(const std::string& path);
    void load(const std::string& path);
};