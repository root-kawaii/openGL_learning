#include "model.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <iostream>
#include <string>
#include <set>

// Constructor definition
Model::Model(const string &path, bool gamma) : gammaCorrection(gamma)
{
    loadModel(path);
}

// Draw method definition
void Model::Draw(Shader &shader)
{
    static vector<glm::mat4> transforms;
    static glm::mat4 identities[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
    float timeSeconds = glfwGetTime(); // Get time directly from GLFW

    // Calculate bone transforms once for the entire model (shared across all meshes)
    transforms.clear();
    if (enableDebugAnimation)
    {

        GetBoneTransformsWithDebugAnim(transforms, timeSeconds);
    }
    else
    {
        GetBoneTransforms(transforms, timeSeconds);
    }

    // Upload bone matrices once (applies to all meshes) using cached location
    GLint location = shader.getUniformLocation("gBones");
    if (location >= 0)
    {
        if (!transforms.empty())
        {
            glUniformMatrix4fv(location, transforms.size(), GL_FALSE, &transforms[0][0][0]);
        }
        else
        {
            // No bones - upload identity matrices
            glUniformMatrix4fv(location, 4, GL_FALSE, &identities[0][0][0]);
        }
    }

    // Draw all meshes (they all use the same global bone array)
    for (unsigned int i = 0; i < meshes.size(); i++)
    {
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

    // 1. diffuse maps
    vector<Mesh_Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
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
    transforms.clear();

    if (!scene || !scene->mAnimations || scene->mNumAnimations == 0)
    {
        static bool printed = false;
        if (!printed) {
            std::cout << "No animations found in scene" << std::endl;
            printed = true;
        }
        return;
    }

    static bool printedAnimInfo = false;
    if (!printedAnimInfo) {
        std::cout << "Animation found: " << scene->mAnimations[0]->mName.C_Str() << std::endl;
        std::cout << "  Duration: " << scene->mAnimations[0]->mDuration << " ticks" << std::endl;
        std::cout << "  Ticks per second: " << scene->mAnimations[0]->mTicksPerSecond << std::endl;
        std::cout << "  Number of channels: " << scene->mAnimations[0]->mNumChannels << std::endl;
        printedAnimInfo = true;
    }

    float ticksPerSecond = (float)(scene->mAnimations[0]->mTicksPerSecond != 0 ? scene->mAnimations[0]->mTicksPerSecond : 25.0);
    float timeInTicks = timeSeconds * ticksPerSecond;
    float animationTime = fmod(timeInTicks, (float)scene->mAnimations[0]->mDuration);

    if (!scene || !scene->mRootNode || m_BoneInfo.empty())
    {
        return;
    }

    // Traverse the entire skeleton hierarchy using cached global inverse transform
    ReadNodeHierarchy(animationTime, m_BoneInfo, scene->mRootNode, m_globalInverseTransform);

    // Collect all bone transforms
    for (const auto &bone : m_BoneInfo)
    {
        transforms.push_back(bone.FinalTransformation);
    }
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