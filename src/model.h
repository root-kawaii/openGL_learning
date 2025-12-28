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
#include <stb_image.h>

using namespace std;

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
    void Draw(Shader &shader);

    vector<Vertex> GetAllVertices() const;

    void SetDiffuseTexture(unsigned int textureID);
    void GetBoneTransforms(vector<glm::mat4> &transforms, float timeSeconds) const;
    void GetBoneTransformsWithDebugAnim(vector<glm::mat4> &transforms, float time) const;

private:
    mutable vector<Vertex> allVerticesCache;
    mutable bool verticesCached = false;

    // Global bone data for the entire model (shared across all meshes)
    map<string, unsigned int> m_BoneNameToIndexMap;
    mutable vector<BoneInfo> m_BoneInfo; // mutable because bone transforms are recalculated each frame
    glm::mat4 m_globalInverseTransform;  // Cached global inverse transform

    void loadModel(const string &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    void parseBones(aiMesh *mesh, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo);
    void parseSingleBone(unsigned int i, aiBone *bone, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo);
    int GetBoneId(const aiBone *pBone, vector<BoneInfo> &boneInfo);
    vector<Mesh_Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName, const aiScene *scene);
    void ReadNodeHierarchy(float animationTime, vector<BoneInfo> &boneInfo, const aiNode *node, const glm::mat4 &parentTransform) const;
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma);
unsigned int TextureFromAssimp(const aiTexture *assimpTexture);
unsigned int loadWhiteTexture();
unsigned int loadDefaultMetallicTexture();
unsigned int loadDefaultRoughnessTexture();