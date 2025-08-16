
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <../json/single_include/nlohmann/json.hpp>
#include <glm/glm.hpp>


struct SceneObject {
    std::string id;
    std::string name;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    
};

class SerializationUtilities{


public:

    SerializationUtilities(){};
    ~SerializationUtilities(){};

    std::vector<SceneObject> getObjects(){ return objects;};
    
    SceneObject* getObjectWithId(const std::string& id) { 
        auto it = objectMap.find(id);
        if (it != objectMap.end()) {
            return it->second;
        }
        return nullptr;
    }


    bool loadScene(const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                return false;
            }
            
            nlohmann::json sceneData;
            file >> sceneData;
            
            return parseScene(sceneData);
        }
        catch (const nlohmann::json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading scene: " << e.what() << std::endl;
            return false;
        }
    }
    
    
    // Parse the JSON data and extract objects
    bool parseScene(const nlohmann::json& sceneData) {
        if (!sceneData.contains("objects") || !sceneData["objects"].is_array()) {
            std::cerr << "Error: Invalid scene format - missing 'objects' array" << std::endl;
            return false;
        }
        
        objects.clear();
        objectMap.clear();
        
        for (const auto& objData : sceneData["objects"]) {
            SceneObject obj = parseObject(objData);
            if (!obj.id.empty()) {
                objects.push_back(obj);
                objectMap[obj.id] = &objects.back();
            }
        }
        
        std::cout << "Successfully loaded " << objects.size() << " objects" << std::endl;
        return true;
    }

        SceneObject parseObject(const nlohmann::json& objData) {
        SceneObject obj;
        
        try {
            // Extract basic properties
            if (objData.contains("id")) {
                obj.id = objData["id"];
            }
            if (objData.contains("name")) {
                obj.name = objData["name"];
            }
            
            // Extract position
            if (objData.contains("position")) {
                const auto& pos = objData["position"];
                obj.position = glm::vec3(
                    pos.value("x", 0.0f),
                    pos.value("y", 0.0f),
                    pos.value("z", 0.0f)
                );
            }
            
            // Extract rotation
            if (objData.contains("rotation")) {
                const auto& rot = objData["rotation"];
                obj.rotation = glm::vec3(
                    rot.value("x", 0.0f),
                    rot.value("y", 0.0f),
                    rot.value("z", 0.0f)
                );
            }
            
            // Extract scale
            if (objData.contains("scale")) {
                const auto& scl = objData["scale"];
                obj.scale = glm::vec3(
                    scl.value("x", 1.0f),
                    scl.value("y", 1.0f),
                    scl.value("z", 1.0f)
                );
            }
            
        }
        catch (const nlohmann::json::exception& e) {
            std::cerr << "Error parsing object: " << e.what() << std::endl;
            return SceneObject(); // Return empty object
        }
        
        return obj;
    }
private:
    std::vector<SceneObject> objects;
    std::unordered_map<std::string, SceneObject*> objectMap;

};