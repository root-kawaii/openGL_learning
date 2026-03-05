#include "model.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <string>
#include <set>
#include "../tracy/public/tracy/Tracy.hpp"

// Constructor definition
Model::Model(const string &path, bool gamma)
    : gammaCorrection(gamma), m_LastUpdateTime(0.0f), m_AnimationsInitialized(false)
{
    loadModel(path);
}

// Constructor for parallel pre-loading: caller already ran importer->ReadFile() on a
// background thread.  processNode() (which creates Meshes + uploads to GPU) runs here,
// on whichever thread calls this constructor — that MUST be the main (GL) thread.
Model::Model(std::shared_ptr<Assimp::Importer> preloaded, const string &path, bool gamma)
    : gammaCorrection(gamma), m_LastUpdateTime(0.0f), m_AnimationsInitialized(false)
{
    importer = preloaded;
    scene    = importer->GetScene();
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cout << "ERROR::ASSIMP (preloaded): " << importer->GetErrorString() << std::endl;
        return;
    }
    aiMatrix4x4 aiRootTransform = scene->mRootNode->mTransformation;
    aiMatrix4x4 aiGlobalInverse = aiRootTransform;
    aiGlobalInverse.Inverse();
    m_globalInverseTransform = glm::transpose(glm::mat4(
        aiGlobalInverse.a1, aiGlobalInverse.a2, aiGlobalInverse.a3, aiGlobalInverse.a4,
        aiGlobalInverse.b1, aiGlobalInverse.b2, aiGlobalInverse.b3, aiGlobalInverse.b4,
        aiGlobalInverse.c1, aiGlobalInverse.c2, aiGlobalInverse.c3, aiGlobalInverse.c4,
        aiGlobalInverse.d1, aiGlobalInverse.d2, aiGlobalInverse.d3, aiGlobalInverse.d4));
    directory = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene);
    InitializeAnimations();
    std::cout << "✓ Model ready (preloaded): " << path << std::endl;
}

// Draw method definition
void Model::Draw(Shader &shader)
{
    ZoneScoped;
    static vector<glm::mat4> transforms;
    static glm::mat4 identities[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
    float timeSeconds = glfwGetTime();

    // Calculate bone transforms once for the entire model (shared across all meshes)
    transforms.clear();
    {
        ZoneScopedN("BoneTransforms");
        if (enableDebugAnimation)
            GetBoneTransformsWithDebugAnim(transforms, timeSeconds);
        else
            GetBoneTransforms(transforms, timeSeconds);
    }

    // Upload bone matrices once (applies to all meshes) using cached location
    {
        ZoneScopedN("BoneUpload_gBones");
        GLint location = shader.getUniformLocation("gBones");
        if (location >= 0)
        {
            if (!transforms.empty())
                glUniformMatrix4fv(location, transforms.size(), GL_FALSE, &transforms[0][0][0]);
            else
                glUniformMatrix4fv(location, 4, GL_FALSE, &identities[0][0][0]);
        }
    }

    // Draw all meshes
    {
        ZoneScopedN("MeshDraw");
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
}

// loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
void Model::loadModel(string const &path)
{
    // read file via ASSIMP (using shared_ptr to keep scene valid)
    importer = std::make_shared<Assimp::Importer>();
    scene = importer->ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    // check for errors
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
    {
        cout << "ERROR::ASSIMP:: " << importer->GetErrorString() << endl;
        return;
    }

    // Debug: Print scene information
    std::cout << "=== Loading Model: " << path << " ===" << std::endl;
    std::cout << "Number of meshes: " << scene->mNumMeshes << std::endl;
    std::cout << "Number of animations: " << scene->mNumAnimations << std::endl;
    std::cout << "Number of materials: " << scene->mNumMaterials << std::endl;

    // Print mesh information
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        std::cout << "Mesh " << i << ": " << scene->mMeshes[i]->mName.C_Str()
                  << " - Bones: " << scene->mMeshes[i]->mNumBones
                  << " - Vertices: " << scene->mMeshes[i]->mNumVertices << std::endl;
    }

    // Convert and cache the global inverse transform
    aiMatrix4x4 aiRootTransform = scene->mRootNode->mTransformation;

    // Debug: print root transformation
    std::cout << "Root node transformation matrix:" << std::endl;
    std::cout << "  [" << aiRootTransform.a1 << " " << aiRootTransform.a2 << " " << aiRootTransform.a3 << " " << aiRootTransform.a4 << "]" << std::endl;
    std::cout << "  [" << aiRootTransform.b1 << " " << aiRootTransform.b2 << " " << aiRootTransform.b3 << " " << aiRootTransform.b4 << "]" << std::endl;
    std::cout << "  [" << aiRootTransform.c1 << " " << aiRootTransform.c2 << " " << aiRootTransform.c3 << " " << aiRootTransform.c4 << "]" << std::endl;
    std::cout << "  [" << aiRootTransform.d1 << " " << aiRootTransform.d2 << " " << aiRootTransform.d3 << " " << aiRootTransform.d4 << "]" << std::endl;

    // Standard approach: invert the root transformation
    aiMatrix4x4 aiGlobalInverse = aiRootTransform;
    aiGlobalInverse.Inverse();
    m_globalInverseTransform = glm::transpose(glm::mat4(
        aiGlobalInverse.a1, aiGlobalInverse.a2, aiGlobalInverse.a3, aiGlobalInverse.a4,
        aiGlobalInverse.b1, aiGlobalInverse.b2, aiGlobalInverse.b3, aiGlobalInverse.b4,
        aiGlobalInverse.c1, aiGlobalInverse.c2, aiGlobalInverse.c3, aiGlobalInverse.c4,
        aiGlobalInverse.d1, aiGlobalInverse.d2, aiGlobalInverse.d3, aiGlobalInverse.d4));

    // retrieve the directory path of the filepath
    directory = path.substr(0, path.find_last_of('/'));

    // process ASSIMP's root node recursively
    processNode(scene->mRootNode, scene);

    // Initialize animation system
    InitializeAnimations();

    // Print final model statistics
    std::cout << "✓ Model loaded: " << path << std::endl;
    std::cout << "  Total vertices: " << getVertexCount() << std::endl;
    std::cout << "  Total triangles: " << getTriangleCount() << std::endl;
    std::cout << "=====================================" << std::endl;
}

// processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
void Model::processNode(aiNode *node, const aiScene *scene)
{
    // process each mesh located at the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        // the node object only contains indices to index the actual objects in the scene.
        // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene)
{
    // data to fill
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<Mesh_Texture> textures;
    vector<BoneInfo> boneInfo;

    // walk through each of the mesh's vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;
        // normals
        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }
        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
            // tangent
            if (mesh->mTangents)
            {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
            }
            // bitangent
            if (mesh->mBitangents)
            {
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
        }
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);

        // Debug logging for first vertex
        if (i == 0)
        {
            std::cout << "First vertex data:" << std::endl;
            std::cout << "  Position: (" << vertex.Position.x << ", " << vertex.Position.y << ", " << vertex.Position.z << ")" << std::endl;
            std::cout << "  Normal: (" << vertex.Normal.x << ", " << vertex.Normal.y << ", " << vertex.Normal.z << ")" << std::endl;
            std::cout << "  TexCoords: (" << vertex.TexCoords.x << ", " << vertex.TexCoords.y << ")" << std::endl;
            std::cout << "  Tangent: (" << vertex.Tangent.x << ", " << vertex.Tangent.y << ", " << vertex.Tangent.z << ")" << std::endl;
            std::cout << "  Bitangent: (" << vertex.Bitangent.x << ", " << vertex.Bitangent.y << ", " << vertex.Bitangent.z << ")" << std::endl;
            std::cout << "  BoneIDs: [" << vertex.m_BoneIDs[0] << ", " << vertex.m_BoneIDs[1] << ", " << vertex.m_BoneIDs[2] << ", " << vertex.m_BoneIDs[3] << "]" << std::endl;
            std::cout << "  Weights: [" << vertex.m_Weights[0] << ", " << vertex.m_Weights[1] << ", " << vertex.m_Weights[2] << ", " << vertex.m_Weights[3] << "]" << std::endl;
        }

        vertices.push_back(vertex);
    }
    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // bones
    parseBones(mesh, vertices, boneInfo);

    // Debug: Print first vertex after bone parsing
    if (!vertices.empty() && mesh->mNumBones > 0) {
        std::cout << "After bone parsing - First vertex:" << std::endl;
        std::cout << "  BoneIDs: [" << vertices[0].m_BoneIDs[0] << ", " << vertices[0].m_BoneIDs[1] << ", " << vertices[0].m_BoneIDs[2] << ", " << vertices[0].m_BoneIDs[3] << "]" << std::endl;
        std::cout << "  Weights: [" << vertices[0].m_Weights[0] << ", " << vertices[0].m_Weights[1] << ", " << vertices[0].m_Weights[2] << ", " << vertices[0].m_Weights[3] << "]" << std::endl;
    }

    // process materials
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
    // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
    // Same applies to other texture as the following list summarizes:
    // diffuse: texture_diffuseN
    // specular: texture_specularN
    // normal: texture_normalN
    // metallic: texture_metallicN
    // roughness: texture_roughnessN

    // 1. diffuse maps (try DIFFUSE first, fall back to BASE_COLOR for PBR/GLB models)
    vector<Mesh_Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
    if (diffuseMaps.empty())
        diffuseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", scene);
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    vector<Mesh_Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Mesh_Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Mesh_Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", scene);
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    // 5. metallic maps
    std::vector<Mesh_Texture> metallicMaps = loadMaterialTextures(material, aiTextureType_METALNESS, "texture_metallic", scene);
    textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());
    // 6. roughness maps
    std::vector<Mesh_Texture> roughnessMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "texture_roughness", scene);
    textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

    // return a mesh object created from the extracted mesh data
    return Mesh(vertices, indices, textures);
}

// checks all material textures of a given type and loads the textures if they're not loaded yet.
// the required info is returned as a Texture struct.
vector<Mesh_Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName, const aiScene *scene)
{
    vector<Mesh_Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++)
        {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
            {
                textures.push_back(textures_loaded[j]);
                skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip)
        { // if texture hasn't been loaded already, load it
            Mesh_Texture texture;

            // Check if this is an embedded texture (GLB files)
            if (str.C_Str()[0] == '*')
            {
                // Embedded texture - extract index
                int textureIndex = std::atoi(str.C_Str() + 1);
                if (textureIndex < scene->mNumTextures)
                {
                    texture.id = TextureFromAssimp(scene->mTextures[textureIndex]);
                    texture.type = typeName;
                    texture.path = str.C_Str();
                    textures.push_back(texture);
                    textures_loaded.push_back(texture);
                }
            }
            else
            {
                // External texture file
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
    }

    // Provide default textures if none are found
    // if (textures.empty())
    // {
    //     Mesh_Texture defaultTex;
    //     if (type == aiTextureType_DIFFUSE)
    //     {
    //         defaultTex.id = loadWhiteTexture();
    //         defaultTex.type = "texture_diffuse";
    //         defaultTex.path = "default_white";
    //     }
    //     else if (type == aiTextureType_METALNESS)
    //     {
    //         defaultTex.id = loadDefaultMetallicTexture();
    //         defaultTex.type = "texture_metallic";
    //         defaultTex.path = "default_metallic";
    //     }
    //     else if (type == aiTextureType_DIFFUSE_ROUGHNESS)
    //     {
    //         defaultTex.id = loadDefaultRoughnessTexture();
    //         defaultTex.type = "texture_roughness";
    //         defaultTex.path = "default_roughness";
    //     }

    //     if (defaultTex.id != 0)
    //     {
    //         textures.push_back(defaultTex);
    //     }
    // }

    return textures;
}

// Global function implementations
unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// New function to handle embedded textures in GLB files
unsigned int TextureFromAssimp(const aiTexture *assimpTexture)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = nullptr;

    if (assimpTexture->mHeight == 0)
    {
        // Compressed texture data (e.g., PNG, JPG embedded in GLB)
        data = stbi_load_from_memory(
            reinterpret_cast<unsigned char *>(assimpTexture->pcData),
            assimpTexture->mWidth, // mWidth contains the actual data size for compressed textures
            &width, &height, &nrComponents, 0);
    }
    else
    {
        // Uncompressed texture data
        width = assimpTexture->mWidth;
        height = assimpTexture->mHeight;
        nrComponents = 4; // aiTexel is always RGBA
        data = reinterpret_cast<unsigned char *>(assimpTexture->pcData);
    }

    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Only free if we used stbi_load_from_memory
        if (assimpTexture->mHeight == 0)
        {
            stbi_image_free(data);
        }
    }
    else
    {
        std::cout << "Failed to load embedded texture" << std::endl;
    }

    return textureID;
}

unsigned int loadWhiteTexture()
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    unsigned char whitePixel[] = {255, 255, 255}; // RGB white

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

unsigned int loadDefaultMetallicTexture()
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    // Default metallic value: 0 (non-metallic)
    unsigned char blackPixel[] = {50, 50, 50}; // RGB black (0 metallic)

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, blackPixel);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

unsigned int loadDefaultRoughnessTexture()
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    // Default roughness value: 0.5 (medium roughness)
    unsigned char grayPixel[] = {20, 20, 20}; // RGB gray (0.5 roughness)

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, grayPixel);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

vector<Vertex> Model::GetAllVertices() const
{
    if (verticesCached)
    {
        return allVerticesCache;
    }

    vector<Vertex> allVertices;

    // Iterate through all meshes and collect all vertices
    for (const auto &mesh : meshes)
    {
        const vector<Vertex> &meshVertices = mesh.vertices;
        allVertices.insert(allVertices.end(), meshVertices.begin(), meshVertices.end());
    }

    // Cache the result for future calls
    allVerticesCache = allVertices;
    verticesCached = true;

    return allVertices;
}
// Add to Model class
void Model::SetDiffuseTexture(unsigned int textureID)
{
    for (auto &mesh : meshes)
    {
        // Find and replace diffuse texture
        for (auto &texture : mesh.textures)
        {
            if (texture.type == "texture_diffuse")
            {
                texture.id = textureID;
                return;
            }
        }

        // If no diffuse texture exists, add one
        Mesh_Texture newTexture;
        newTexture.id = textureID;
        newTexture.type = "texture_diffuse";
        newTexture.path = ""; // Custom texture
        mesh.textures.push_back(newTexture);
    }
}

void Model::parseBones(aiMesh *mesh, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo)
{
    if (mesh->mNumBones > 0) {
        std::cout << "Parsing bones for mesh '" << mesh->mName.C_Str() << "': " << mesh->mNumBones << " bones found" << std::endl;
    }

    for (unsigned int i = 0; i < mesh->mNumBones; i++)
    {
        parseSingleBone(i, mesh->mBones[i], vertices, boneInfo);
    }

    if (mesh->mNumBones == 0) {
        std::cout << "WARNING: Mesh '" << mesh->mName.C_Str() << "' has NO bones!" << std::endl;
    }
}

int Model::GetBoneId(const aiBone *pBone, vector<BoneInfo> &boneInfo)
{
    int BoneIndex = 0;
    string BoneName(pBone->mName.C_Str());

    if (m_BoneNameToIndexMap.find(BoneName) == m_BoneNameToIndexMap.end())
    {
        // Allocate an index for a new bone in the GLOBAL bone array
        BoneIndex = (int)m_BoneNameToIndexMap.size();
        m_BoneNameToIndexMap[BoneName] = BoneIndex;
    }
    else
    {
        BoneIndex = m_BoneNameToIndexMap[BoneName];
    }

    return BoneIndex;
}

void Model::parseSingleBone(unsigned int i, aiBone *bone, vector<Vertex> &vertices, vector<BoneInfo> &boneInfo)
{
    int BoneId = GetBoneId(bone, boneInfo);

    std::cout << "  Bone " << i << ": '" << bone->mName.C_Str() << "' - GlobalID: " << BoneId << " - Weights: " << bone->mNumWeights << std::endl;

    // Add to global bone array if this is a new bone
    if (BoneId == m_BoneInfo.size())
    {
        // Convert aiMatrix4x4 to glm::mat4 (transpose for row-major to column-major)
        aiMatrix4x4 aiOffset = bone->mOffsetMatrix;
        glm::mat4 offsetMatrix = glm::transpose(glm::mat4(
            aiOffset.a1, aiOffset.a2, aiOffset.a3, aiOffset.a4,
            aiOffset.b1, aiOffset.b2, aiOffset.b3, aiOffset.b4,
            aiOffset.c1, aiOffset.c2, aiOffset.c3, aiOffset.c4,
            aiOffset.d1, aiOffset.d2, aiOffset.d3, aiOffset.d4));

        BoneInfo newBone(offsetMatrix);
        m_BoneInfo.push_back(newBone);
        std::cout << "    Added new bone to global array (total: " << m_BoneInfo.size() << ")" << std::endl;
    }

    // Assign bone weights to vertices
    int weightsAssigned = 0;
    for (unsigned int j = 0; j < bone->mNumWeights; j++)
    {
        unsigned int vertexID = bone->mWeights[j].mVertexId;
        float weight = bone->mWeights[j].mWeight;

        Vertex &vertex = vertices[vertexID];

        for (int k = 0; k < MAX_BONE_INFLUENCE; k++)
        {
            if (vertex.m_Weights[k] == 0.0f)
            {
                vertex.m_BoneIDs[k] = BoneId; // This is now a global bone index
                vertex.m_Weights[k] = weight;
                weightsAssigned++;
                break;
            }
        }
    }
    std::cout << "    Assigned " << weightsAssigned << " weights to vertices" << std::endl;
}

void Model::GetBoneTransforms(vector<glm::mat4> &transforms, float timeSeconds) const
{
    // Calculate delta time
    float deltaTime = timeSeconds - m_LastUpdateTime;
    m_LastUpdateTime = timeSeconds;

    // Update animation state (advance time, handle blending)
    const_cast<Model*>(this)->UpdateAnimationState(deltaTime);

    // Get transforms based on current state
    GetBoneTransformsInternal(transforms);
}

void Model::GetBoneTransformsWithDebugAnim(vector<glm::mat4> &transforms, float time) const
{
    transforms.clear();

    static bool printedDebugInfo = false;
    if (!printedDebugInfo) {
        std::cout << "Debug Animation Active - Animating specific bones procedurally" << std::endl;
        std::cout << "  Bones being animated: Arms (L1, L2, R1, R2) and Flaps (1, 2, 3)" << std::endl;
        printedDebugInfo = true;
    }

    if (m_BoneInfo.empty())
        return;

    if (!scene || !scene->mRootNode)
        return;

    // First calculate normal bone transforms using cached global inverse transform
    // For debug animation, we use time=0 to get bind pose, then apply procedural animations
    ReadNodeHierarchy(0.0f, m_BoneInfo, scene->mRootNode, m_globalInverseTransform);

    // Build a map of bone names we want to animate
    static std::set<std::string> animatedBones = {
        "ArmL1_01", "ArmL2_02",            // Left arm
        "ArmR1_07", "ArmR2_08",            // Right arm
        "Flap1_06", "Flap2_05", "Flap3_04" // Flaps for extra movement
    };

    // Then apply exaggerated animation on top
    for (size_t i = 0; i < m_BoneInfo.size(); i++)
    {
        glm::mat4 finalTransform = m_BoneInfo[i].FinalTransformation;

        // Find bone name for this index
        std::string boneName;
        for (const auto &bone_pair : m_BoneNameToIndexMap)
        {
            if (bone_pair.second == i)
            {
                boneName = bone_pair.first;
                break;
            }
        }

        // Only animate specific bones
        if (animatedBones.find(boneName) != animatedBones.end())
        {
            // Create exaggerated rotation animations
            float angle = sin(time * 2.0f + i * 0.5f) * 1.0f; // Exaggerated angle

            // Different rotation axes for different bones
            glm::vec3 rotationAxis;
            if (boneName.find("Arm") != std::string::npos)
            {
                // Arms rotate around Z axis (side to side)
                rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
                angle *= 1.5f; // Extra exaggeration for arms
            }
            else if (boneName.find("Flap") != std::string::npos)
            {
                // Flaps rotate around Y axis
                rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            else
            {
                rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, rotationAxis);

            // Apply rotation AFTER the bone transform (in bone space)
            finalTransform = finalTransform * rotation;
        }

        transforms.push_back(finalTransform);
    }
}

void Model::ReadNodeHierarchy(float animationTime, vector<BoneInfo> &boneInfo, const aiNode *pNode, const glm::mat4 &ParentTransform) const
{
    string NodeName(pNode->mName.data);

    const aiAnimation *pAnimation = scene->mAnimations[0];

    // Start with the static node transformation
    aiMatrix4x4 aiNodeTransform = pNode->mTransformation;
    glm::mat4 NodeTransformation = glm::transpose(glm::mat4(
        aiNodeTransform.a1, aiNodeTransform.a2, aiNodeTransform.a3, aiNodeTransform.a4,
        aiNodeTransform.b1, aiNodeTransform.b2, aiNodeTransform.b3, aiNodeTransform.b4,
        aiNodeTransform.c1, aiNodeTransform.c2, aiNodeTransform.c3, aiNodeTransform.c4,
        aiNodeTransform.d1, aiNodeTransform.d2, aiNodeTransform.d3, aiNodeTransform.d4));

    // Check if this node has an animation channel
    const aiNodeAnim *pNodeAnim = nullptr;
    for (uint i = 0; i < pAnimation->mNumChannels; i++)
    {
        if (string(pAnimation->mChannels[i]->mNodeName.data) == NodeName)
        {
            pNodeAnim = pAnimation->mChannels[i];
            break;
        }
    }

    // If this node is animated, interpolate between keyframes
    if (pNodeAnim)
    {
        // Interpolate position
        aiVector3D position(0.0f, 0.0f, 0.0f);
        if (pNodeAnim->mNumPositionKeys > 0)
        {
            // For simplicity, just use the first key (you should interpolate based on animationTime)
            position = pNodeAnim->mPositionKeys[0].mValue;

            // Find the keyframes to interpolate between
            for (uint i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
            {
                if (animationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
                {
                    uint nextKey = i + 1;
                    float deltaTime = (float)(pNodeAnim->mPositionKeys[nextKey].mTime - pNodeAnim->mPositionKeys[i].mTime);
                    float factor = (animationTime - (float)pNodeAnim->mPositionKeys[i].mTime) / deltaTime;

                    const aiVector3D &start = pNodeAnim->mPositionKeys[i].mValue;
                    const aiVector3D &end = pNodeAnim->mPositionKeys[nextKey].mValue;
                    position = start + (end - start) * factor;
                    break;
                }
            }
        }

        // Interpolate rotation
        aiQuaternion rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (pNodeAnim->mNumRotationKeys > 0)
        {
            rotation = pNodeAnim->mRotationKeys[0].mValue;

            for (uint i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
            {
                if (animationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
                {
                    uint nextKey = i + 1;
                    float deltaTime = (float)(pNodeAnim->mRotationKeys[nextKey].mTime - pNodeAnim->mRotationKeys[i].mTime);
                    float factor = (animationTime - (float)pNodeAnim->mRotationKeys[i].mTime) / deltaTime;

                    const aiQuaternion &start = pNodeAnim->mRotationKeys[i].mValue;
                    const aiQuaternion &end = pNodeAnim->mRotationKeys[nextKey].mValue;
                    aiQuaternion::Interpolate(rotation, start, end, factor);
                    rotation.Normalize();
                    break;
                }
            }
        }

        // Interpolate scaling
        aiVector3D scaling(1.0f, 1.0f, 1.0f);
        if (pNodeAnim->mNumScalingKeys > 0)
        {
            scaling = pNodeAnim->mScalingKeys[0].mValue;

            for (uint i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
            {
                if (animationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
                {
                    uint nextKey = i + 1;
                    float deltaTime = (float)(pNodeAnim->mScalingKeys[nextKey].mTime - pNodeAnim->mScalingKeys[i].mTime);
                    float factor = (animationTime - (float)pNodeAnim->mScalingKeys[i].mTime) / deltaTime;

                    const aiVector3D &start = pNodeAnim->mScalingKeys[i].mValue;
                    const aiVector3D &end = pNodeAnim->mScalingKeys[nextKey].mValue;
                    scaling = start + (end - start) * factor;
                    break;
                }
            }
        }

        // Combine into transformation matrix
        aiMatrix4x4 matTranslation, matRotation, matScaling;
        aiMatrix4x4::Translation(position, matTranslation);
        matRotation = aiMatrix4x4(rotation.GetMatrix());
        aiMatrix4x4::Scaling(scaling, matScaling);

        aiNodeTransform = matTranslation * matRotation * matScaling;

        NodeTransformation = glm::transpose(glm::mat4(
            aiNodeTransform.a1, aiNodeTransform.a2, aiNodeTransform.a3, aiNodeTransform.a4,
            aiNodeTransform.b1, aiNodeTransform.b2, aiNodeTransform.b3, aiNodeTransform.b4,
            aiNodeTransform.c1, aiNodeTransform.c2, aiNodeTransform.c3, aiNodeTransform.c4,
            aiNodeTransform.d1, aiNodeTransform.d2, aiNodeTransform.d3, aiNodeTransform.d4));
    }

    glm::mat4 GlobalTransformation = ParentTransform * NodeTransformation;

    auto it = m_BoneNameToIndexMap.find(NodeName);
    if (it != m_BoneNameToIndexMap.end())
    {
        uint BoneIndex = it->second;
        // Check if this bone index is valid for this mesh
        if (BoneIndex < boneInfo.size())
        {
            boneInfo[BoneIndex].FinalTransformation = m_globalInverseTransform * GlobalTransformation * boneInfo[BoneIndex].OffsetMatrix;
        }
    }

    for (uint i = 0; i < pNode->mNumChildren; i++)
    {
        ReadNodeHierarchy(animationTime, boneInfo, pNode->mChildren[i], GlobalTransformation);
    }
}

// ============================================================================
// Multi-Animation System Implementation
// ============================================================================

void Model::InitializeAnimations()
{
    m_AnimationNameToIndexMap.clear();
    m_AnimationsInitialized = true;
    m_LastUpdateTime = 0.0f;

    if (!scene || !scene->mAnimations || scene->mNumAnimations == 0) {
        // Model has no animations - that's fine, transforms will just be empty
        return;
    }

    // Build name-to-index mapping and per-animation channel maps
    m_AnimChannelMaps.resize(scene->mNumAnimations);
    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        const aiAnimation* anim = scene->mAnimations[i];
        std::string name(anim->mName.C_Str());
        m_AnimationNameToIndexMap[name] = i;

        auto& channelMap = m_AnimChannelMaps[i];
        for (unsigned int c = 0; c < anim->mNumChannels; ++c)
            channelMap[anim->mChannels[c]->mNodeName.C_Str()] = anim->mChannels[c];

        std::cout << "Registered animation: '" << name << "' (index " << i << ", "
                  << anim->mNumChannels << " channels)" << std::endl;
    }

    // Build flat node list in BFS/DFS order so parentIdx always < childIdx.
    // This lets the per-frame loop run linearly without recursion or string hashing.
    unsigned int numAnims = scene->mNumAnimations;
    m_NodeList.clear();
    // Use a stack of (aiNode*, parentIdx)
    std::vector<std::pair<const aiNode*, int>> stack;
    stack.push_back({scene->mRootNode, -1});
    while (!stack.empty()) {
        auto [node, parentIdx] = stack.back();
        stack.pop_back();

        NodeEntry entry;
        entry.node      = node;
        entry.parentIdx = parentIdx;
        // Resolve bone index once
        auto boneIt = m_BoneNameToIndexMap.find(node->mName.C_Str());
        entry.boneIdx = (boneIt != m_BoneNameToIndexMap.end()) ? (int)boneIt->second : -1;
        // Resolve channel pointer for every animation
        entry.channels.resize(numAnims, nullptr);
        for (unsigned int i = 0; i < numAnims; ++i) {
            auto& cm = m_AnimChannelMaps[i];
            auto chanIt = cm.find(node->mName.C_Str());
            if (chanIt != cm.end())
                entry.channels[i] = chanIt->second;
        }

        int myIdx = (int)m_NodeList.size();
        m_NodeList.push_back(std::move(entry));

        // Push children (reverse order to maintain DFS left-to-right)
        for (int c = (int)node->mNumChildren - 1; c >= 0; --c)
            stack.push_back({node->mChildren[c], myIdx});
    }
    m_GlobalTransforms.resize(m_NodeList.size());

    // Set default animation to first one
    m_CurrentAnimation.animationIndex = 0;
    m_CurrentAnimation.currentTime = 0.0f;
    m_CurrentAnimation.isLooping = true;
    m_CurrentAnimation.playbackSpeed = 1.0f;
    m_CurrentAnimation.isPaused = false;
}

void Model::ReadNodeHierarchyForAnimation(
    float animTime,
    vector<BoneInfo>& boneInfo,
    const aiNode* pNode,
    const glm::mat4& ParentTransform,
    const unordered_map<string, const aiNodeAnim*>& channelMap) const
{
    string NodeName(pNode->mName.data);

    // Start with the static node transformation
    aiMatrix4x4 aiNodeTransform = pNode->mTransformation;
    glm::mat4 NodeTransformation = glm::transpose(glm::mat4(
        aiNodeTransform.a1, aiNodeTransform.a2, aiNodeTransform.a3, aiNodeTransform.a4,
        aiNodeTransform.b1, aiNodeTransform.b2, aiNodeTransform.b3, aiNodeTransform.b4,
        aiNodeTransform.c1, aiNodeTransform.c2, aiNodeTransform.c3, aiNodeTransform.c4,
        aiNodeTransform.d1, aiNodeTransform.d2, aiNodeTransform.d3, aiNodeTransform.d4));

    // Fix 1: O(1) channel lookup — channelMap passed directly, no pointer search needed.
    const aiNodeAnim* pNodeAnim = nullptr;
    {
        auto chanIt = channelMap.find(NodeName);
        if (chanIt != channelMap.end())
            pNodeAnim = chanIt->second;
    }

    // Helper: binary search for the keyframe just before animTime (Fix 2).
    // Returns the index of the last key whose time <= animTime.
    auto findKeyIndex = [](double time, auto* keys, unsigned int count) -> unsigned int {
        // upper_bound finds first key with .mTime > time; step back one.
        unsigned int lo = 0, hi = count - 1;
        while (lo < hi) {
            unsigned int mid = (lo + hi + 1) / 2;
            if ((double)keys[mid].mTime <= time)
                lo = mid;
            else
                hi = mid - 1;
        }
        return lo;
    };

    // If this node is animated, interpolate between keyframes
    if (pNodeAnim) {
        // Interpolate scaling
        aiVector3D scaling(1, 1, 1);
        if (pNodeAnim->mNumScalingKeys == 1) {
            scaling = pNodeAnim->mScalingKeys[0].mValue;
        } else if (pNodeAnim->mNumScalingKeys > 1) {
            uint scalingIndex     = findKeyIndex(animTime, pNodeAnim->mScalingKeys, pNodeAnim->mNumScalingKeys - 1);
            uint nextScalingIndex = scalingIndex + 1;
            float deltaTime = (float)(pNodeAnim->mScalingKeys[nextScalingIndex].mTime -
                                      pNodeAnim->mScalingKeys[scalingIndex].mTime);
            float factor    = (animTime - (float)pNodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;
            const aiVector3D& start = pNodeAnim->mScalingKeys[scalingIndex].mValue;
            const aiVector3D& end   = pNodeAnim->mScalingKeys[nextScalingIndex].mValue;
            scaling = start + factor * (end - start);
        }

        // Interpolate rotation
        aiQuaternion rotation(1, 0, 0, 0);
        if (pNodeAnim->mNumRotationKeys == 1) {
            rotation = pNodeAnim->mRotationKeys[0].mValue;
        } else if (pNodeAnim->mNumRotationKeys > 1) {
            uint rotationIndex     = findKeyIndex(animTime, pNodeAnim->mRotationKeys, pNodeAnim->mNumRotationKeys - 1);
            uint nextRotationIndex = rotationIndex + 1;
            float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotationIndex].mTime -
                                      pNodeAnim->mRotationKeys[rotationIndex].mTime);
            float factor    = (animTime - (float)pNodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;
            const aiQuaternion& start = pNodeAnim->mRotationKeys[rotationIndex].mValue;
            const aiQuaternion& end   = pNodeAnim->mRotationKeys[nextRotationIndex].mValue;
            aiQuaternion::Interpolate(rotation, start, end, factor);
            rotation.Normalize();
        }

        // Interpolate position
        aiVector3D position(0, 0, 0);
        if (pNodeAnim->mNumPositionKeys == 1) {
            position = pNodeAnim->mPositionKeys[0].mValue;
        } else if (pNodeAnim->mNumPositionKeys > 1) {
            uint positionIndex     = findKeyIndex(animTime, pNodeAnim->mPositionKeys, pNodeAnim->mNumPositionKeys - 1);
            uint nextPositionIndex = positionIndex + 1;
            float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPositionIndex].mTime -
                                      pNodeAnim->mPositionKeys[positionIndex].mTime);
            float factor    = (animTime - (float)pNodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;
            const aiVector3D& start = pNodeAnim->mPositionKeys[positionIndex].mValue;
            const aiVector3D& end   = pNodeAnim->mPositionKeys[nextPositionIndex].mValue;
            position = start + factor * (end - start);
        }

        // Combine transformations
        aiMatrix4x4 scalingMatrix, rotationMatrix, translationMatrix;
        aiMatrix4x4::Scaling(scaling, scalingMatrix);
        rotationMatrix = aiMatrix4x4(rotation.GetMatrix());
        aiMatrix4x4::Translation(position, translationMatrix);
        aiNodeTransform = translationMatrix * rotationMatrix * scalingMatrix;

        NodeTransformation = glm::transpose(glm::mat4(
            aiNodeTransform.a1, aiNodeTransform.a2, aiNodeTransform.a3, aiNodeTransform.a4,
            aiNodeTransform.b1, aiNodeTransform.b2, aiNodeTransform.b3, aiNodeTransform.b4,
            aiNodeTransform.c1, aiNodeTransform.c2, aiNodeTransform.c3, aiNodeTransform.c4,
            aiNodeTransform.d1, aiNodeTransform.d2, aiNodeTransform.d3, aiNodeTransform.d4));
    }

    glm::mat4 GlobalTransformation = ParentTransform * NodeTransformation;

    // Update bone final transform if this node is a bone
    auto it = m_BoneNameToIndexMap.find(NodeName);
    if (it != m_BoneNameToIndexMap.end()) {
        uint BoneIndex = it->second;
        if (BoneIndex < boneInfo.size()) {
            boneInfo[BoneIndex].FinalTransformation = m_globalInverseTransform *
                                                     GlobalTransformation *
                                                     boneInfo[BoneIndex].OffsetMatrix;
        }
    }

    // Recursively process child nodes
    for (uint i = 0; i < pNode->mNumChildren; i++) {
        ReadNodeHierarchyForAnimation(animTime, boneInfo, pNode->mChildren[i],
                                     GlobalTransformation, channelMap);
    }
}

void Model::ComputeBoneTransformsForAnimation(
    vector<glm::mat4>& transforms,
    unsigned int animIndex,
    float animTime) const
{
    transforms.clear();

    if (!scene || !scene->mAnimations || animIndex >= scene->mNumAnimations) {
        return;
    }

    // Flat linear loop — no recursion, no string hashing, no map lookups per frame.
    auto findKeyIndex = [](double time, auto* keys, unsigned int count) -> unsigned int {
        unsigned int lo = 0, hi = count - 1;
        while (lo < hi) {
            unsigned int mid = (lo + hi + 1) / 2;
            if ((double)keys[mid].mTime <= time) lo = mid;
            else hi = mid - 1;
        }
        return lo;
    };

    for (int ni = 0; ni < (int)m_NodeList.size(); ++ni) {
        const NodeEntry& entry = m_NodeList[ni];
        const aiNode* pNode   = entry.node;

        // Static node transform
        const aiMatrix4x4& t = pNode->mTransformation;
        glm::mat4 nodeTransform = glm::transpose(glm::mat4(
            t.a1, t.a2, t.a3, t.a4,
            t.b1, t.b2, t.b3, t.b4,
            t.c1, t.c2, t.c3, t.c4,
            t.d1, t.d2, t.d3, t.d4));

        // Override with animated transform if this node has a channel
        const aiNodeAnim* ch = entry.channels[animIndex];
        if (ch) {
            aiVector3D scaling(1,1,1);
            if (ch->mNumScalingKeys == 1) {
                scaling = ch->mScalingKeys[0].mValue;
            } else if (ch->mNumScalingKeys > 1) {
                uint si = findKeyIndex(animTime, ch->mScalingKeys, ch->mNumScalingKeys - 1);
                float dt = (float)(ch->mScalingKeys[si+1].mTime - ch->mScalingKeys[si].mTime);
                float f  = (animTime - (float)ch->mScalingKeys[si].mTime) / dt;
                scaling = ch->mScalingKeys[si].mValue + f * (ch->mScalingKeys[si+1].mValue - ch->mScalingKeys[si].mValue);
            }

            aiQuaternion rotation(1,0,0,0);
            if (ch->mNumRotationKeys == 1) {
                rotation = ch->mRotationKeys[0].mValue;
            } else if (ch->mNumRotationKeys > 1) {
                uint ri = findKeyIndex(animTime, ch->mRotationKeys, ch->mNumRotationKeys - 1);
                float dt = (float)(ch->mRotationKeys[ri+1].mTime - ch->mRotationKeys[ri].mTime);
                float f  = (animTime - (float)ch->mRotationKeys[ri].mTime) / dt;
                aiQuaternion::Interpolate(rotation, ch->mRotationKeys[ri].mValue, ch->mRotationKeys[ri+1].mValue, f);
                rotation.Normalize();
            }

            aiVector3D position(0,0,0);
            if (ch->mNumPositionKeys == 1) {
                position = ch->mPositionKeys[0].mValue;
            } else if (ch->mNumPositionKeys > 1) {
                uint pi = findKeyIndex(animTime, ch->mPositionKeys, ch->mNumPositionKeys - 1);
                float dt = (float)(ch->mPositionKeys[pi+1].mTime - ch->mPositionKeys[pi].mTime);
                float f  = (animTime - (float)ch->mPositionKeys[pi].mTime) / dt;
                position = ch->mPositionKeys[pi].mValue + f * (ch->mPositionKeys[pi+1].mValue - ch->mPositionKeys[pi].mValue);
            }

            aiMatrix4x4 ms, mr, mp;
            aiMatrix4x4::Scaling(scaling, ms);
            mr = aiMatrix4x4(rotation.GetMatrix());
            aiMatrix4x4::Translation(position, mp);
            aiMatrix4x4 ai = mp * mr * ms;
            nodeTransform = glm::transpose(glm::mat4(
                ai.a1, ai.a2, ai.a3, ai.a4,
                ai.b1, ai.b2, ai.b3, ai.b4,
                ai.c1, ai.c2, ai.c3, ai.c4,
                ai.d1, ai.d2, ai.d3, ai.d4));
        }

        glm::mat4 parent = (entry.parentIdx < 0) ? m_globalInverseTransform
                                                  : m_GlobalTransforms[entry.parentIdx];
        m_GlobalTransforms[ni] = parent * nodeTransform;

    }

    // Collect bone transforms in boneIdx order — matches what the shader expects.
    transforms.assign(m_BoneInfo.size(), glm::mat4(1.0f));
    for (int ni = 0; ni < (int)m_NodeList.size(); ++ni) {
        const NodeEntry& entry = m_NodeList[ni];
        if (entry.boneIdx >= 0 && entry.boneIdx < (int)m_BoneInfo.size()) {
            transforms[entry.boneIdx] = m_globalInverseTransform *
                                        m_GlobalTransforms[ni] *
                                        m_BoneInfo[entry.boneIdx].OffsetMatrix;
        }
    }
}

void Model::UpdateSingleAnimationTime(AnimationState& animState, float deltaTime)
{
    if (animState.animationIndex < 0 || !scene ||
        animState.animationIndex >= (int)scene->mNumAnimations) {
        return;
    }

    const aiAnimation* pAnimation = scene->mAnimations[animState.animationIndex];
    float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ?
                                   pAnimation->mTicksPerSecond : 25.0);

    // Advance time in ticks
    float deltaTimeTicks = deltaTime * ticksPerSecond * animState.playbackSpeed;
    animState.currentTime += deltaTimeTicks;

    // Handle looping
    float duration = (float)pAnimation->mDuration;
    if (animState.isLooping) {
        animState.currentTime = fmod(animState.currentTime, duration);
    } else {
        // Clamp to duration if not looping
        if (animState.currentTime > duration) {
            animState.currentTime = duration;
            animState.isPaused = true; // Auto-pause at end
        }
    }
}

void Model::UpdateAnimationState(float deltaTime)
{
    if (m_CurrentAnimation.isPaused) {
        return;
    }

    if (m_BlendState.isBlending) {
        // Update blend progress
        m_BlendState.blendElapsed += deltaTime;
        m_BlendState.blendFactor = std::min(1.0f,
            m_BlendState.blendElapsed / m_BlendState.blendDuration);

        // Advance both source and target animations
        UpdateSingleAnimationTime(m_BlendState.sourceAnim, deltaTime);
        UpdateSingleAnimationTime(m_BlendState.targetAnim, deltaTime);

        // Check if blend is complete
        if (m_BlendState.blendFactor >= 1.0f) {
            // Blend complete - switch to target animation
            m_CurrentAnimation = m_BlendState.targetAnim;
            m_BlendState.isBlending = false;
        }
    } else {
        // Check if animation is about to loop and should blend to beginning
        if (m_CurrentAnimation.isLooping && m_CurrentAnimation.animationIndex >= 0 && scene) {
            const aiAnimation* pAnimation = scene->mAnimations[m_CurrentAnimation.animationIndex];
            float duration = (float)pAnimation->mDuration;
            float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ? pAnimation->mTicksPerSecond : 25.0);

            // Blend threshold: 0.3 seconds before end (in ticks)
            float blendThresholdTicks = 0.3f * ticksPerSecond;
            float timeUntilEnd = duration - m_CurrentAnimation.currentTime;

            // If we're close to the end, start blending to the beginning
            if (timeUntilEnd <= blendThresholdTicks && timeUntilEnd > 0) {
                // Setup blend from current position to beginning of same animation
                m_BlendState.isBlending = true;
                m_BlendState.sourceAnim = m_CurrentAnimation;
                m_BlendState.targetAnim = m_CurrentAnimation;
                m_BlendState.targetAnim.currentTime = 0.0f; // Target is the beginning
                m_BlendState.blendDuration = timeUntilEnd / ticksPerSecond; // Blend over remaining time
                m_BlendState.blendElapsed = 0.0f;
                m_BlendState.blendFactor = 0.0f;
                return; // Let next frame handle the blend
            }
        }

        // Store previous time for event checking
        float previousTime = m_CurrentAnimation.currentTime;

        // Update current animation time
        UpdateSingleAnimationTime(m_CurrentAnimation, deltaTime);

        // Check for events that should trigger
        CheckAnimationEvents(previousTime, m_CurrentAnimation.currentTime);
    }
}

void Model::BlendBoneTransforms(
    const vector<glm::mat4>& t1,
    const vector<glm::mat4>& t2,
    float blendFactor,
    vector<glm::mat4>& out) const
{
    out.clear();

    size_t boneCount = std::min(t1.size(), t2.size());
    out.reserve(boneCount);

    for (size_t i = 0; i < boneCount; ++i) {
        // Extract translation
        glm::vec3 pos1 = glm::vec3(t1[i][3]);
        glm::vec3 pos2 = glm::vec3(t2[i][3]);

        // Extract scale by computing length of each basis vector
        glm::vec3 scale1 = glm::vec3(
            glm::length(glm::vec3(t1[i][0])),
            glm::length(glm::vec3(t1[i][1])),
            glm::length(glm::vec3(t1[i][2]))
        );
        glm::vec3 scale2 = glm::vec3(
            glm::length(glm::vec3(t2[i][0])),
            glm::length(glm::vec3(t2[i][1])),
            glm::length(glm::vec3(t2[i][2]))
        );

        // Extract rotation (normalize the rotation matrix to remove scale)
        glm::mat3 rotMat1 = glm::mat3(
            glm::vec3(t1[i][0]) / scale1.x,
            glm::vec3(t1[i][1]) / scale1.y,
            glm::vec3(t1[i][2]) / scale1.z
        );
        glm::mat3 rotMat2 = glm::mat3(
            glm::vec3(t2[i][0]) / scale2.x,
            glm::vec3(t2[i][1]) / scale2.y,
            glm::vec3(t2[i][2]) / scale2.z
        );
        glm::quat rot1 = glm::quat_cast(rotMat1);
        glm::quat rot2 = glm::quat_cast(rotMat2);

        // Interpolate
        glm::vec3 finalPos = glm::mix(pos1, pos2, blendFactor);
        glm::quat finalRot = glm::slerp(rot1, rot2, blendFactor);
        glm::vec3 finalScale = glm::mix(scale1, scale2, blendFactor);

        // Reconstruct matrix
        glm::mat4 finalTransform = glm::mat4(1.0f);
        finalTransform = glm::translate(finalTransform, finalPos);
        finalTransform *= glm::mat4_cast(finalRot);
        finalTransform = glm::scale(finalTransform, finalScale);

        out.push_back(finalTransform);
    }
}

void Model::GetBoneTransformsInternal(vector<glm::mat4>& transforms) const
{
    if (m_BlendState.isBlending) {
        // Compute transforms for source animation
        vector<glm::mat4> sourceTransforms;
        ComputeBoneTransformsForAnimation(
            sourceTransforms,
            m_BlendState.sourceAnim.animationIndex,
            m_BlendState.sourceAnim.currentTime
        );

        // Compute transforms for target animation
        vector<glm::mat4> targetTransforms;
        ComputeBoneTransformsForAnimation(
            targetTransforms,
            m_BlendState.targetAnim.animationIndex,
            m_BlendState.targetAnim.currentTime
        );

        // Blend the two
        BlendBoneTransforms(sourceTransforms, targetTransforms,
                           m_BlendState.blendFactor, transforms);
    } else {
        // No blending - just compute for current animation
        ComputeBoneTransformsForAnimation(
            transforms,
            m_CurrentAnimation.animationIndex,
            m_CurrentAnimation.currentTime
        );
    }
}

// ============================================================================
// Public API: Animation Control
// ============================================================================

void Model::PlayAnimation(const std::string& animationName, float blendDuration)
{
    auto it = m_AnimationNameToIndexMap.find(animationName);
    if (it == m_AnimationNameToIndexMap.end()) {
        std::cerr << "Animation '" << animationName << "' not found!" << std::endl;
        return;
    }

    PlayAnimationByIndex(it->second, blendDuration);
}

void Model::PlayAnimationByIndex(unsigned int animIndex, float blendDuration)
{
    if (!scene || animIndex >= scene->mNumAnimations) {
        std::cerr << "Invalid animation index: " << animIndex << std::endl;
        return;
    }

    // If already playing this animation, do nothing
    if (!m_BlendState.isBlending && m_CurrentAnimation.animationIndex == (int)animIndex) {
        return;
    }

    if (blendDuration <= 0.0f || m_CurrentAnimation.animationIndex < 0) {
        // No blending - instant switch
        m_CurrentAnimation.animationIndex = animIndex;
        m_CurrentAnimation.currentTime = 0.0f;
        m_CurrentAnimation.isPaused = false;
        m_BlendState.isBlending = false;
    } else {
        // Setup blend
        m_BlendState.isBlending = true;
        m_BlendState.sourceAnim = m_CurrentAnimation;
        m_BlendState.targetAnim = AnimationState();
        m_BlendState.targetAnim.animationIndex = animIndex;
        m_BlendState.targetAnim.currentTime = 0.0f;
        m_BlendState.targetAnim.playbackSpeed = m_CurrentAnimation.playbackSpeed;
        m_BlendState.targetAnim.isLooping = m_CurrentAnimation.isLooping;
        m_BlendState.blendDuration = blendDuration;
        m_BlendState.blendElapsed = 0.0f;
        m_BlendState.blendFactor = 0.0f;
    }
}

std::string Model::GetCurrentAnimationName() const
{
    if (m_CurrentAnimation.animationIndex < 0 || !scene) {
        return "";
    }
    return std::string(scene->mAnimations[m_CurrentAnimation.animationIndex]->mName.C_Str());
}

std::vector<std::string> Model::GetAvailableAnimations() const
{
    std::vector<std::string> names;
    for (const auto& pair : m_AnimationNameToIndexMap) {
        names.push_back(pair.first);
    }
    return names;
}

bool Model::HasAnimation(const std::string& animationName) const
{
    return m_AnimationNameToIndexMap.find(animationName) != m_AnimationNameToIndexMap.end();
}

// ============================================================================
// Public API: Playback Control
// ============================================================================

void Model::PauseAnimation()
{
    m_CurrentAnimation.isPaused = true;
}

void Model::ResumeAnimation()
{
    m_CurrentAnimation.isPaused = false;
}

bool Model::IsAnimationPaused() const
{
    return m_CurrentAnimation.isPaused;
}

void Model::SetAnimationSpeed(float speed)
{
    m_CurrentAnimation.playbackSpeed = speed;
}

float Model::GetAnimationSpeed() const
{
    return m_CurrentAnimation.playbackSpeed;
}

void Model::SetAnimationLooping(bool loop)
{
    m_CurrentAnimation.isLooping = loop;
}

bool Model::IsAnimationLooping() const
{
    return m_CurrentAnimation.isLooping;
}

float Model::GetAnimationTime() const
{
    if (m_CurrentAnimation.animationIndex < 0 || !scene) {
        return 0.0f;
    }

    const aiAnimation* pAnimation = scene->mAnimations[m_CurrentAnimation.animationIndex];
    float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ?
                                   pAnimation->mTicksPerSecond : 25.0);

    // Convert ticks to seconds
    return m_CurrentAnimation.currentTime / ticksPerSecond;
}

float Model::GetAnimationDuration() const
{
    if (m_CurrentAnimation.animationIndex < 0 || !scene) {
        return 0.0f;
    }

    const aiAnimation* pAnimation = scene->mAnimations[m_CurrentAnimation.animationIndex];
    float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ?
                                   pAnimation->mTicksPerSecond : 25.0);

    // Convert ticks to seconds
    return (float)pAnimation->mDuration / ticksPerSecond;
}

void Model::SetAnimationTime(float timeInSeconds)
{
    if (m_CurrentAnimation.animationIndex < 0 || !scene) {
        return;
    }

    const aiAnimation* pAnimation = scene->mAnimations[m_CurrentAnimation.animationIndex];
    float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ?
                                   pAnimation->mTicksPerSecond : 25.0);

    // Convert seconds to ticks
    m_CurrentAnimation.currentTime = timeInSeconds * ticksPerSecond;

    // Clamp to valid range
    float duration = (float)pAnimation->mDuration;
    m_CurrentAnimation.currentTime = std::max(0.0f, std::min(m_CurrentAnimation.currentTime, duration));
}

// ============================================================================
// Public API: Event System
// ============================================================================

void Model::RegisterAnimationEvent(
    const std::string& animationName,
    float timeInSeconds,
    std::function<void()> callback)
{
    m_AnimationEvents.emplace_back(animationName, timeInSeconds, callback);
}

void Model::CheckAnimationEvents(float previousTime, float currentTime)
{
    if (m_CurrentAnimation.animationIndex < 0) return;

    const aiAnimation* pAnimation = scene->mAnimations[m_CurrentAnimation.animationIndex];
    std::string currentAnimName(pAnimation->mName.C_Str());

    float ticksPerSecond = (float)(pAnimation->mTicksPerSecond != 0 ?
                                   pAnimation->mTicksPerSecond : 25.0);

    // Convert ticks to seconds
    float prevTimeSec = previousTime / ticksPerSecond;
    float currTimeSec = currentTime / ticksPerSecond;

    for (auto& event : m_AnimationEvents) {
        // Only check events for current animation
        if (event.animationName != currentAnimName) continue;

        // Check if event time is between previous and current time
        bool crossedEvent = false;

        if (prevTimeSec < currTimeSec) {
            // Normal forward playback
            crossedEvent = (event.triggerTime >= prevTimeSec &&
                          event.triggerTime < currTimeSec);
        } else {
            // Looped back to start
            crossedEvent = (event.triggerTime >= prevTimeSec ||
                          event.triggerTime < currTimeSec);
        }

        if (crossedEvent && !event.hasTriggered) {
            event.callback();
            event.hasTriggered = true;
        }

        // Reset trigger flag if we've passed the event time by a lot
        if (currTimeSec < event.triggerTime - 0.1f) {
            event.hasTriggered = false;
        }
    }
}

void Model::ClearAnimationEvents(const std::string& animationName)
{
    m_AnimationEvents.erase(
        std::remove_if(m_AnimationEvents.begin(), m_AnimationEvents.end(),
            [&animationName](const AnimationEvent& e) {
                return e.animationName == animationName;
            }),
        m_AnimationEvents.end()
    );
}

void Model::ClearAllAnimationEvents()
{
    m_AnimationEvents.clear();
}