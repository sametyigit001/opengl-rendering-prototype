#include "Model.h"
#include "Ray.h"
#include <iostream>
#include <filesystem>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"

static std::string resolveTexturePath(const std::string& rawPath, const std::string& modelDir) {
    std::filesystem::path raw(rawPath);

    if (std::filesystem::exists(raw) && !std::filesystem::is_directory(raw)) {
        return raw.string();
    }

    std::string filename = raw.filename().string();
    std::filesystem::path mDir(modelDir);
    std::filesystem::path parentDir = mDir.parent_path();

    std::vector<std::filesystem::path> candidates = {
        mDir / raw,
        mDir / filename,
        mDir / "textures" / filename,
        parentDir / "textures" / filename,
        parentDir / filename
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) && !std::filesystem::is_directory(candidate)) {
            return candidate.string();
        }
    }

    if (std::filesystem::exists(parentDir) && std::filesystem::is_directory(parentDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(parentDir)) {
            if (entry.is_regular_file() && entry.path().filename() == filename) {
                return entry.path().string();
            }
        }
    }

    return "";
}

Model::Model(const std::string& path) {
    Model::loadModel(path);
}

void Model::loadModel(std::string path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals|aiProcess_PreTransformVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
        return;
    }

    directory = std::filesystem::path(path).parent_path().string();

    m_AABB.reset();
    m_nodes.clear();
    m_meshes.clear();

    Model::processNode(scene->mRootNode, scene,-1);

    float targetSize = 5.0f;
    float maxDim = m_AABB.getMaxExtent();
    float scaleFactor = (maxDim > 0.0001f) ? (targetSize / maxDim) : 1.0f;
    glm::vec3 center = m_AABB.getCenter();

    
    normMat = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
    normMat = glm::translate(normMat, -center);
}

void Model::draw(Shader& shader) {
    for (auto& submesh : m_meshes) {
        shader.setInt("u_isSelected", submesh.isSelected ? 1 : 0);
        if (submesh.material) {
            submesh.geometry.Draw(shader, *submesh.material);
        }
    }
}

int Model::processNode(aiNode* node, const aiScene* scene,int parentIndex) {
    modelNode engineNode;
    engineNode.name = node->mName.C_Str();
    engineNode.parentIndex = parentIndex;

    int currentIndex = static_cast<int>(m_nodes.size());

    m_nodes.push_back(engineNode);
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(Model::processMesh(mesh, scene));

        m_nodes[currentIndex].meshIndices.push_back(static_cast<int>(m_meshes.size() - 1));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
       int childIndex = processNode(node->mChildren[i], scene,currentIndex);

       m_nodes[currentIndex].childIndices.push_back(childIndex);
    }
    return currentIndex;
}

subMesh Model::processMesh(aiMesh* aiMesh, const aiScene* scene) {
    const int vertexCount = static_cast<int>(aiMesh->mNumVertices);
    const int faceCount = static_cast<int>(aiMesh->mNumFaces);

    std::vector<meshVertex> vertices(vertexCount);
    std::vector<unsigned int> indices(faceCount * 3);

    #pragma omp parallel for
    for (int i = 0; i < aiMesh->mNumVertices; i++) {
        meshVertex vertex;
        glm::vec3 vector;
        vector.x = aiMesh->mVertices[i].x;
        vector.y = aiMesh->mVertices[i].y;
        vector.z = aiMesh->mVertices[i].z;
        vertex.position = vector;

        if (aiMesh->HasNormals()) {
            vector.x = aiMesh->mNormals[i].x;
            vector.y = aiMesh->mNormals[i].y;
            vector.z = aiMesh->mNormals[i].z;
            vertex.normal = vector;
        }
        else {
            vertex.normal = glm::vec3(0.0f);
        }

        if (aiMesh->mTextureCoords[0]) {
            glm::vec2 vector;
            vector.x = aiMesh->mTextureCoords[0][i].x;
            vector.y = aiMesh->mTextureCoords[0][i].y;
            vertex.texCoords = vector;
        }
        else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices[i] = vertex;
    }
    #pragma omp parallel for
    for (int i = 0; i < faceCount; i++) {
        const aiFace& face = aiMesh->mFaces[i];
        const int idx = i * 3;
        indices[idx] = face.mIndices[0];
        indices[idx + 1] = face.mIndices[1];
        indices[idx + 2] = face.mIndices[2];
    }

    AABB submeshAABB;

    for (const auto& v : vertices) {
        m_AABB.expand(v.position);
        submeshAABB.expand(v.position);
    }

    const float rayPadding = 0.005f;
    submeshAABB.min -= glm::vec3(rayPadding);
    submeshAABB.max += glm::vec3(rayPadding);

    std::shared_ptr<Material> mat = nullptr;
    if (aiMesh->mMaterialIndex >= 0) {
        aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];

        auto diffuseMap = loadMaterialTexture(aiMat, aiTextureType_DIFFUSE);
        auto specularMap = loadMaterialTexture(aiMat, aiTextureType_SPECULAR);
        auto opacityMap = loadMaterialTexture(aiMat, aiTextureType_OPACITY);

        float shininess = 32.0f;
        aiMat->Get(AI_MATKEY_SHININESS, shininess);
        if (shininess < 1.0f) {
            shininess = 32.0f;
        }
        aiColor3D specColor(0.5f, 0.5f, 0.5f);
        aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specColor);
        glm::vec3 specularColor(specColor.r, specColor.g, specColor.b);
        if (glm::length(specularColor) < 0.01f) {
            specularColor = glm::vec3(0.5f);
        }
        

        mat = std::make_shared<Material>(diffuseMap, specularMap,opacityMap, shininess, specularColor);
    }
    return subMesh{ mesh(std::move(vertices), std::move(indices)), mat,submeshAABB,false };
}
std::shared_ptr<Texture> Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type) {
    if (mat->GetTextureCount(type) == 0) {
        if (type == aiTextureType_DIFFUSE && mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
            type = aiTextureType_BASE_COLOR;
        }
        else {
            return nullptr;
        }
    }

    aiString str;
    if (mat->GetTexture(type, 0, &str) != AI_SUCCESS || str.length == 0) {
        return nullptr;
    }

    std::string rawPath = str.C_Str();
    std::cout << "[Assimp] Raw texture path in model: " << rawPath << std::endl;

    if (!rawPath.empty() && rawPath[0] == '*') {
        return nullptr;
    }

    std::string resolvedPath = resolveTexturePath(rawPath, this->directory);
    if (resolvedPath.empty()) {
        std::cerr << "[Texture Error] Could not find file on disk: "
            << std::filesystem::path(rawPath).filename().string() << std::endl;
        return nullptr;
    }

    std::cout << "[Assimp] Resolved texture path: " << resolvedPath << std::endl;

    std::string canonicalPath = std::filesystem::canonical(resolvedPath).string();
    auto it = m_loadedTextures.find(canonicalPath);
    if (it != m_loadedTextures.end()) {
        return it->second;
    }

    auto texture = std::make_shared<Texture>(canonicalPath);
    m_loadedTextures[canonicalPath] = texture;
    return texture;
}
void Model::selectNode(int index)
{
    for (auto& sm : m_meshes) {
        sm.isSelected = false;
    }

    if (index >= 0 && index < m_nodes.size()) {
        for (int meshIdx : m_nodes[index].meshIndices) {
            if (meshIdx >= 0 && meshIdx < m_meshes.size()) {
                m_meshes[meshIdx].isSelected = true;
            }
        }
    }
}

bool Model::pickSubMeshes(const ray& localRay, float& outDist) {
    float closestDist = (std::numeric_limits<float>::max)();
    subMesh* hitMesh = nullptr;

    float globalDist = 0.0f;
    if (!m_AABB.intersects(localRay, globalDist)) {
        std::cout << "[Broad-Phase] Failed Global AABB. Ray completely missed the model bounds.\n";
        for (auto& sm : m_meshes) sm.isSelected = false;
        return false;
    }

    std::cout << "[Broad-Phase] Passed Global AABB! Checking submeshes...\n";

    int submeshHits = 0;
    int triHits = 0;

    for (auto& sm : m_meshes) {
        sm.isSelected = false;
        float aabbDist = 0.0f;

        if (sm.localAABB.intersects(localRay, aabbDist)) {
            submeshHits++;
            const auto& vertices = sm.geometry.getVertices();
            const auto& indices = sm.geometry.getIndices();

            bool hitTriangle = false;
            float closestTriDist = (std::numeric_limits<float>::max)();

            if (!indices.empty()) {
                for (size_t i = 0; i < indices.size(); i += 3) {
                    glm::vec3 v0 = vertices[indices[i]].position;
                    glm::vec3 v1 = vertices[indices[i + 1]].position;
                    glm::vec3 v2 = vertices[indices[i + 2]].position;

                    float triDist = 0.0f;
                    if (intersectTriangle(localRay, v0, v1, v2, triDist)) {
                        triHits++;
                        if (triDist < closestTriDist) {
                            closestTriDist = triDist;
                            hitTriangle = true;
                        }
                    }
                }
            }
            else {
                for (size_t i = 0; i < vertices.size(); i += 3) {
                    glm::vec3 v0 = vertices[i].position;
                    glm::vec3 v1 = vertices[i + 1].position;
                    glm::vec3 v2 = vertices[i + 2].position;

                    float triDist = 0.0f;
                    if (intersectTriangle(localRay, v0, v1, v2, triDist)) {
                        triHits++;
                        if (triDist < closestTriDist) {
                            closestTriDist = triDist;
                            hitTriangle = true;
                        }
                    }
                }
            }

            if (hitTriangle && closestTriDist < closestDist) {
                closestDist = closestTriDist;
                hitMesh = &sm;
            }
        }
    }

    std::cout << "[Narrow-Phase] Submesh AABBs Hit: " << submeshHits
        << " | Total Triangles Hit: " << triHits << "\n";

    if (hitMesh) {
        hitMesh->isSelected = true;
        outDist = closestDist;
        return true;
    }

    return false;
}