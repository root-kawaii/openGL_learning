#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <../src/mesh.h>
#include "shader_m.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <stb_image.h>
#include <mutex>
#include <unordered_map>

using namespace std;

// Animation state tracking
struct AnimationState {
    int animationIndex;
    float currentTime;
    float playbackSpeed;
    bool isLooping;
    bool isPaused;

    AnimationState()
        : animationIndex(-1), currentTime(0.0f),
          playbackSpeed(1.0f), isLooping(true), isPaused(false) {}
};

// Blend state tracking
struct BlendState {
    bool isBlending;
    AnimationState sourceAnim;
    AnimationState targetAnim;
    float blendFactor;
    float blendDuration;
    float blendElapsed;

    BlendState()
        : isBlending(false), blendFactor(0.0f),
          blendDuration(0.0f), blendElapsed(0.0f) {}
};

// Animation event
struct AnimationEvent {
    std::string animationName;
    float triggerTime;
    std::function<void()> callback;
    bool hasTriggered;

    AnimationEvent(const std::string& name, float time, std::function<void()> cb)
        : animationName(name), triggerTime(time), callback(cb), hasTriggered(false) {}
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);
unsigned int TextureFromAssimp(const aiTexture *assimpTexture);
unsigned int loadWhiteTexture();
unsigned int loadDefaultMetallicTexture();
unsigned int loadDefaultRoughnessTexture();

class Model
{
public:
    vector<Mesh_Texture> textures_loaded;
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;

    std::shared_ptr<Assimp::Importer> importer;
    const aiScene *scene = NULL;

    // Debug animation flag
    bool enableDebugAnimation = false;

    Model(const string &path, bool gamma = false);
    // Takes an already-ReadFile'd importer (used for parallel pre-loading).
    // processNode / GPU upload still happen on the calling thread.
    Model(std::shared_ptr<Assimp::Importer> preloaded, const string &path, bool gamma = false);

    void Draw(Shader &shader);

    vector<Vertex> GetAllVertices() const;

    // Get model statistics for performance debugging
    int getTriangleCount() const
    {
        int total = 0;
        for (const auto &mesh : meshes)
        {
            total += mesh.indices.size() / 3;
        }
        return total;
    }

    int getVertexCount() const
    {
        int total = 0;
        for (const auto &mesh : meshes)
        {
            total += mesh.vertices.size();
        }
        return total;
    }

    void SetDiffuseTexture(unsigned int textureID);
    void GetBoneTransforms(vector<glm::mat4> &transforms, float timeSeconds) const;
    void GetBoneTransformsWithDebugAnim(vector<glm::mat4> &transforms, float time) const;

    // ====== Animation Selection ======
    void PlayAnimation(const std::string& animationName, float blendDuration = 0.3f);
    void PlayAnimationByIndex(unsigned int animIndex, float blendDuration = 0.3f);
    std::string GetCurrentAnimationName() const;
    std::vector<std::string> GetAvailableAnimations() const;
    bool HasAnimation(const std::string& animationName) const;

    // ====== Playback Control ======
    void PauseAnimation();
    void ResumeAnimation();
    bool IsAnimationPaused() const;
    void SetAnimationSpeed(float speed);
    float GetAnimationSpeed() const;
    void SetAnimationLooping(bool loop);
    bool IsAnimationLooping() const;
    float GetAnimationTime() const;
    float GetAnimationDuration() const;
    void SetAnimationTime(float timeInSeconds);

    // ====== Event System ======
    void RegisterAnimationEvent(const std::string& animationName, float timeInSeconds, std::function<void()> callback);
    void ClearAnimationEvents(const std::string& animationName);
    void ClearAllAnimationEvents();

private:
    mutable vector<Vertex> allVerticesCache;
    mutable bool verticesCached = false;

    // Global bone data for the entire model (shared across all meshes)
    map<string, unsigned int> m_BoneNameToIndexMap;
    mutable vector<BoneInfo> m_BoneInfo; // mutable because bone transforms are recalculated each frame
    glm::mat4 m_globalInverseTransform;  // Cached global inverse transform

    // Animation system state
    std::map<std::string, unsigned int> m_AnimationNameToIndexMap;
    AnimationState m_CurrentAnimation;
    BlendState m_BlendState;
    std::vector<AnimationEvent> m_AnimationEvents;
    mutable float m_LastUpdateTime;
    bool m_AnimationsInitialized;

    void loadModel(const string &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    void parseBones(aiMesh *mesh, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo);
    void parseSingleBone(unsigned int i, aiBone *bone, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo);
    int GetBoneId(const aiBone *pBone, vector<BoneInfo> &boneInfo);
    vector<Mesh_Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName, const aiScene *scene);
    void ReadNodeHierarchy(float animationTime, vector<BoneInfo> &boneInfo, const aiNode *node, const glm::mat4 &parentTransform) const;

    // Animation system private helpers
    void InitializeAnimations();
    void UpdateAnimationState(float deltaTime);
    void UpdateSingleAnimationTime(AnimationState& animState, float deltaTime);
    void CheckAnimationEvents(float previousTime, float currentTime);
    void ComputeBoneTransformsForAnimation(vector<glm::mat4>& transforms, unsigned int animIndex, float animTime) const;
    void ReadNodeHierarchyForAnimation(float animTime, vector<BoneInfo>& boneInfo, const aiNode* node, const glm::mat4& parentTransform, const aiAnimation* anim) const;
    void BlendBoneTransforms(const vector<glm::mat4>& t1, const vector<glm::mat4>& t2, float factor, vector<glm::mat4>& out) const;
    void GetBoneTransformsInternal(vector<glm::mat4>& transforms) const;
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma);
unsigned int TextureFromAssimp(const aiTexture *assimpTexture);
unsigned int loadWhiteTexture();
unsigned int loadDefaultMetallicTexture();
unsigned int loadDefaultRoughnessTexture();