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

    Model(const string &path, bool gamma = false);
    void Draw(Shader &shader);

    vector<Vertex> GetAllVertices() const;

    void SetDiffuseTexture(unsigned int textureID);

private:
    mutable vector<Vertex> allVerticesCache;
    mutable bool verticesCached = false;

    void loadModel(const string &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    vector<Mesh_Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName, const aiScene *scene);
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma);
unsigned int TextureFromAssimp(const aiTexture *assimpTexture);
unsigned int loadWhiteTexture();
unsigned int loadDefaultMetallicTexture();
unsigned int loadDefaultRoughnessTexture();