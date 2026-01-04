#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <../json/single_include/nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "game_object.h"

struct SceneObject
{
    std::string id;
    std::string name;
    std::string path;
    std::string shader_name;
    glm::vec3 color;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    float collisionRadius;
    bool gameEntity;
};

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    // Add more light properties as needed
};

class SerializationUtilities
{

public:
    SerializationUtilities() {};
    ~SerializationUtilities() {};

    std::vector<SceneObject> getObjects() { return objects; };
    std::vector<Light> getLights() { return lights; };

    SceneObject *getObjectWithId(const std::string &id)
    {
        auto it = objectMap.find(id);
        if (it != objectMap.end())
        {
            return it->second;
        }
        return nullptr;
    }

    bool loadScene(const std::string &filename)
    {
        try
        {
            std::ifstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                return false;
            }

            nlohmann::json sceneData;
            file >> sceneData;

            return parseScene(sceneData);
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading scene: " << e.what() << std::endl;
            return false;
        }
    }

    // Parse the JSON data and extract objects
    bool parseScene(const nlohmann::json &sceneData)
    {
        if (!sceneData.contains("objects") || !sceneData["objects"].is_array())
        {
            std::cerr << "Error: Invalid scene format - missing 'objects' array" << std::endl;
            return false;
        }

        objects.clear();
        objectMap.clear();
        lights.clear();

        for (const auto &objData : sceneData["objects"])
        {
            SceneObject obj = parseObject(objData);
            if (!obj.id.empty())
            {
                objects.push_back(obj);
                objectMap[obj.id] = &objects.back();
            }
        }

        // Parse lights if present
        if (sceneData.contains("lights") && sceneData["lights"].is_array())
        {
            for (const auto &lightData : sceneData["lights"])
            {
                Light light = parseLight(lightData);
                lights.push_back(light);
            }
            std::cout << "Successfully loaded " << lights.size() << " lights" << std::endl;
        }

        std::cout << "Successfully loaded " << objects.size() << " objects" << std::endl;
        return true;
    }

    SceneObject parseObject(const nlohmann::json &objData)
    {
        SceneObject obj;
        obj.collisionRadius = 0;
        obj.shader_name = "default";
        obj.gameEntity = false;

        try
        {
            // Extract basic properties
            if (objData.contains("id"))
            {
                obj.id = objData["id"];
            }
            if (objData.contains("name"))
            {
                obj.name = objData["name"];
            }
            if (objData.contains("path"))
            {
                obj.path = objData["path"];
            }
            if (objData.contains("shader_name"))
            {
                obj.shader_name = objData["shader_name"];
            }

            // Extract position
            if (objData.contains("position"))
            {
                const auto &pos = objData["position"];
                obj.position = glm::vec3(
                    pos.value("x", 0.0f),
                    pos.value("y", 0.0f),
                    pos.value("z", 0.0f));
            }

            // Extract rotation
            if (objData.contains("rotation"))
            {
                const auto &rot = objData["rotation"];
                obj.rotation = glm::vec3(
                    rot.value("x", 0.0f),
                    rot.value("y", 0.0f),
                    rot.value("z", 0.0f));
            }

            // Extract scale
            if (objData.contains("scale"))
            {
                const auto &scl = objData["scale"];
                obj.scale = glm::vec3(
                    scl.value("x", 1.0f),
                    scl.value("y", 1.0f),
                    scl.value("z", 1.0f));
            }
            if (objData.contains("color"))
            {
                const auto &scl = objData["color"];
                obj.color = glm::vec3(
                    scl.value("r", 1.0f),
                    scl.value("g", 1.0f),
                    scl.value("b", 1.0f));
            }
            if (objData.contains("entity"))
            {
                obj.gameEntity = true;
            }
            // Extract collision radius
            if (objData.contains("collision_radius"))
            {
                const auto &scl = objData["collision_radius"];
                obj.collisionRadius = scl.value("v", 0.0f);
            }
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "Error parsing object: " << e.what() << std::endl;
            return SceneObject(); // Return empty object
        }

        return obj;
    }

    Light parseLight(const nlohmann::json &lightData)
    {
        Light light;
        light.intensity = 1.0f; // Default intensity

        try
        {
            // Extract position
            if (lightData.contains("position"))
            {
                const auto &pos = lightData["position"];
                light.position = glm::vec3(
                    pos.value("x", 0.0f),
                    pos.value("y", 0.0f),
                    pos.value("z", 0.0f));
            }

            // Extract color
            if (lightData.contains("color"))
            {
                const auto &col = lightData["color"];
                light.color = glm::vec3(
                    col.value("r", 1.0f),
                    col.value("g", 1.0f),
                    col.value("b", 1.0f));
            }
            else
            {
                light.color = glm::vec3(1.0f, 1.0f, 1.0f); // Default white
            }

            // Extract intensity
            if (lightData.contains("intensity"))
            {
                light.intensity = lightData["intensity"];
            }
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "Error parsing light: " << e.what() << std::endl;
            return Light(); // Return empty light
        }

        return light;
    }

    bool saveScene(const std::string &filename, const std::vector<std::shared_ptr<GameObject>> &objects, const std::vector<Light> &sceneLights)
    {
        try
        {
            nlohmann::json sceneData;
            sceneData["objects"] = nlohmann::json::array();

            for (const auto &objPtr : objects)
            {
                if (!objPtr || objPtr->name.rfind("Light_", 0) == 0)
                {
                    std::cerr << "Warning: skipping null GameObject in saveScene\n";
                    continue; // prevent crash
                }

                const auto &obj = *objPtr; // safe now
                nlohmann::json objData;

                objData["id"] = obj.name;
                // objData["name"] = std::to_string(obj.ID);
                objData["path"] = obj.modelPath;
                objData["shader_name"] = obj.shaderName;
                if (obj.gameEntity != "")
                {
                    objData["entity"] = obj.gameEntity;
                }

                objData["position"] = {
                    {"x", obj.position.x},
                    {"y", obj.position.y},
                    {"z", obj.position.z}};

                objData["rotation"] = {
                    {"x", obj.rotation.x},
                    {"y", obj.rotation.y},
                    {"z", obj.rotation.z}};

                objData["scale"] = {
                    {"x", obj.scale.x},
                    {"y", obj.scale.y},
                    {"z", obj.scale.z}};

                objData["color"] = {
                    {"r", obj.color.x},
                    {"g", obj.color.y},
                    {"b", obj.color.z}};

                objData["collision_radius"] = {
                    {"v", obj.collisionRadius}};

                sceneData["objects"].push_back(objData);
            }

            // Serialize lights
            sceneData["lights"] = nlohmann::json::array();
            for (const auto &light : sceneLights)
            {
                nlohmann::json lightData;

                lightData["position"] = {
                    {"x", light.position.x},
                    {"y", light.position.y},
                    {"z", light.position.z}};

                lightData["color"] = {
                    {"r", light.color.r},
                    {"g", light.color.g},
                    {"b", light.color.b}};

                lightData["intensity"] = light.intensity;

                sceneData["lights"].push_back(lightData);
            }

            std::ofstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
                return false;
            }

            file << sceneData.dump(4); // pretty-print JSON
            file.close();

            std::cout << "Scene saved successfully to " << filename << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error saving scene: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::vector<SceneObject> objects;
    std::unordered_map<std::string, SceneObject *> objectMap;
    std::vector<Light> lights;
};