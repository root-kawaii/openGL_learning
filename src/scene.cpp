#include "scene.h"



    Scene::Scene(){
        entityCounter = 0;
    }


    Scene::~Scene(){

        // TBD
    }


    uint32_t Scene::addGameObject(std::shared_ptr<GameObject> gameObject) {
        // Better ID generation
        uint32_t id = generateUniqueId();
        gameObject->ID = id;
        
        // Add to both containers
        objectsById[id] = gameObject.get();
        gameObjects.push_back(std::move(gameObject));
        
        return id;
    }

    void Scene::destroyGameObject(GameObject* obj){
        obj->~GameObject();
    }

    // linear lookup time, not made for frequent use
    GameObject* Scene::findObjectByName(const std::string& name){
        for (const auto& gameObject : gameObjects) {
            if (gameObject->name == name) {
                return gameObject.get();
            }
        }
        return nullptr;  // Not found
    }

    // constant lookup
    GameObject* Scene::findObjectById(uint32_t id){
        auto it = objectsById.find(id);
        return (it != objectsById.end()) ? it->second : nullptr;
    }

    
    uint32_t Scene::generateUniqueId() {
        return ++entityCounter;
    }
