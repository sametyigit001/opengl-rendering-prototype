#define GLM_ENABLE_EXPERIMENTAL
#include "Model.h"
#include "Ray.h"
#include "glm/gtx/quaternion.hpp"
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
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
        return;
    }

    directory = std::filesystem::path(path).parent_path().string();

    m_AABB.reset();
    m_nodes.clear();
    m_meshes.clear();

    Model::processNode(scene->mRootNode, scene,-1);

    // Build bounds in model space after applying each node's imported transform.
    std::vector<glm::mat4> nodeWorld(m_nodes.size(), glm::mat4(1.0f));
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const modelNode& node = m_nodes[i];

        glm::mat4 local(1.0f);
        local = glm::translate(local, node.position);
        local = glm::rotate(local, glm::radians(node.rotation.x), { 1.0f, 0.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.y), { 0.0f, 1.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.z), { 0.0f, 0.0f, 1.0f });
        local = glm::scale(local, node.scale);

        nodeWorld[i] = node.parentIndex >= 0
            ? nodeWorld[node.parentIndex] * local
            : local;

        for (int meshIndex : node.meshIndices) {
            if (meshIndex >= 0 && static_cast<size_t>(meshIndex) < m_meshes.size()) {
                m_AABB.expand(transformAABB(m_meshes[meshIndex].localAABB, nodeWorld[i]));
            }
        }
    }

    float targetSize = 5.0f;
    float maxDim = m_AABB.getMaxExtent();
    float scaleFactor = (maxDim > 0.0001f) ? (targetSize / maxDim) : 1.0f;
    glm::vec3 center = m_AABB.getCenter();

    
    normMat = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
    normMat = glm::translate(normMat, -center);
}

void Model::draw(Shader& shader) {
    std::vector<glm::mat4> nodeWorld(m_nodes.size(), glm::mat4(1.0f));
    const glm::mat4 modelWorld = getModelMatrix();
    const bool updateBounds = isChanged;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const modelNode& node = m_nodes[i];

        glm::mat4 local(1.0f);
        local = glm::translate(local, node.position);
        local = glm::rotate(local, glm::radians(node.rotation.x), { 1.0f, 0.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.y), { 0.0f, 1.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.z), { 0.0f, 0.0f, 1.0f });
        local = glm::scale(local, node.scale);

        if (node.parentIndex >= 0) {
            nodeWorld[i] = nodeWorld[node.parentIndex] * local;
        }
        else {
            nodeWorld[i] = local;
        }

        for (int meshIndex : node.meshIndices) {
            subMesh& submesh = m_meshes[meshIndex];

            if (updateBounds) {
                submesh.modelAABB =
                    transformAABB(submesh.localAABB, nodeWorld[i]);
            }

            shader.setUniformModel(modelWorld * nodeWorld[i]);
            shader.setInt("u_isSelected", submesh.isSelected ? 1 : 0);

            if (submesh.material) {
                submesh.geometry.Draw(shader, *submesh.material);
            }
        }
    }

    if (updateBounds) {
        isChanged = false;
    }
}

int Model::processNode(aiNode* node, const aiScene* scene,int parentIndex) {
    modelNode engineNode;
    engineNode.name = node->mName.C_Str();
    engineNode.parentIndex = parentIndex;
    aiVector3D aiPosition;
    aiVector3D aiScale;
    aiQuaternion aiRotation;

    node->mTransformation.Decompose(aiScale, aiRotation, aiPosition);

    engineNode.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
    engineNode.scale = glm::vec3(aiScale.x, aiScale.y, aiScale.z);
    glm::quat rotation(aiRotation.w,aiRotation.x, aiRotation.y, aiRotation.z);
    engineNode.rotation = glm::degrees(glm::eulerAngles(rotation));


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
        submeshAABB.expand(v.position);
    }

    const float rayPadding = 0.005f;
    submeshAABB.min -= glm::vec3(rayPadding);
    submeshAABB.max += glm::vec3(rayPadding);

    std::shared_ptr<Material> mat = nullptr;
    if (aiMesh->mMaterialIndex >= 0) {
        aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];

        auto diffuseMap = loadMaterialTexture(aiMat, aiTextureType_DIFFUSE,scene);
        auto specularMap = loadMaterialTexture(aiMat, aiTextureType_SPECULAR,scene);
        auto opacityMap = loadMaterialTexture(aiMat, aiTextureType_OPACITY,scene);

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
std::shared_ptr<Texture> Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene) {
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
        // Assimp's embedded texture references look like "*0", "*1", etc.
        const int textureIndex = std::stoi(rawPath.substr(1));

        if (textureIndex < 0 ||
            textureIndex >= static_cast<int>(scene->mNumTextures)) {
            return nullptr;
        }

        const aiTexture* embedded = scene->mTextures[textureIndex];

        if (embedded->mHeight == 0) {
            // Compressed PNG/JPEG/etc. data: decode it with stb_image.
            int width = 0;
            int height = 0;
            int channels = 0;

            unsigned char* pixels = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(embedded->pcData),
                static_cast<int>(embedded->mWidth), // compressed byte count
                &width,
                &height,
                &channels,
                0);

            if (!pixels) {
                return nullptr;
            }

            auto texture = std::make_shared<Texture>(pixels, width, height, channels);
            stbi_image_free(pixels);
            return texture;
        }

        // Uncompressed Assimp texels need conversion to RGBA bytes
        // before passing them to Texture.
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


int Model::pickSubMeshes(const ray& localRay, float& outDist) {
    float closestDist = (std::numeric_limits<float>::max)();
    int hitMeshIndex = -1;

    // Build the accumulated node transform for each mesh.
    std::vector<glm::mat4> nodeWorld(m_nodes.size(), glm::mat4(1.0f));
    std::vector<glm::mat4> meshNodeWorld(m_meshes.size(), glm::mat4(1.0f));

    for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex) {
        const modelNode& node = m_nodes[nodeIndex];

        glm::mat4 local(1.0f);
        local = glm::translate(local, node.position);
        local = glm::rotate(
            local, glm::radians(node.rotation.x), { 1.0f, 0.0f, 0.0f });
        local = glm::rotate(
            local, glm::radians(node.rotation.y), { 0.0f, 1.0f, 0.0f });
        local = glm::rotate(
            local, glm::radians(node.rotation.z), { 0.0f, 0.0f, 1.0f });
        local = glm::scale(local, node.scale);
        nodeWorld[nodeIndex] = node.parentIndex >= 0
            ? nodeWorld[node.parentIndex] * local
            : local;

        for (int meshIndex : node.meshIndices) {
            if (meshIndex >= 0 &&
                static_cast<size_t>(meshIndex) < meshNodeWorld.size()) {
                meshNodeWorld[meshIndex] = nodeWorld[nodeIndex];
            }
        }
    }

    int submeshHits = 0;
    int triHits = 0;

    for (size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex) {
        subMesh& sm = m_meshes[meshIndex];


        const glm::mat4& nodeMatrix = meshNodeWorld[meshIndex];

        // Keep localAABB unchanged; refresh its transformed model-space copy.
        sm.modelAABB = transformAABB(sm.localAABB, nodeMatrix);

        float aabbDist = 0.0f;
        if (!sm.modelAABB.intersects(localRay, aabbDist)) {
            continue;
        }

        ++submeshHits;

        const auto& vertices = sm.geometry.getVertices();
        const auto& indices = sm.geometry.getIndices();

        auto toModelSpace = [&nodeMatrix](const glm::vec3& point) {
            return glm::vec3(nodeMatrix * glm::vec4(point, 1.0f));
            };

        bool hitTriangle = false;
        float closestTriDist = (std::numeric_limits<float>::max)();

        auto testTriangle = [&](size_t i0, size_t i1, size_t i2) {
            const glm::vec3 v0 = toModelSpace(vertices[i0].position);
            const glm::vec3 v1 = toModelSpace(vertices[i1].position);
            const glm::vec3 v2 = toModelSpace(vertices[i2].position);

            float triDist = 0.0f;
            if (intersectTriangle(localRay, v0, v1, v2, triDist)) {
                ++triHits;
                if (triDist < closestTriDist) {
                    closestTriDist = triDist;
                    hitTriangle = true;
                }
            }
            };

        if (!indices.empty()) {
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                testTriangle(indices[i], indices[i + 1], indices[i + 2]);
            }
        }
        else {
            for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
                testTriangle(i, i + 1, i + 2);
            }
        }

        if (hitTriangle && closestTriDist < closestDist) {
            closestDist = closestTriDist;
            hitMeshIndex = static_cast<int>(meshIndex);
        }
    }

    std::cout << "[Narrow-Phase] Submesh AABBs Hit: " << submeshHits
        << " | Total Triangles Hit: " << triHits << "\n";

    if (hitMeshIndex >= 0) {
        outDist = closestDist;
    }

    return hitMeshIndex;
}

AABB Model::transformAABB(const AABB& local, const glm::mat4& nodeWorld)
{
    AABB result; // min/max start at extreme values

    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                glm::vec3 corner(
                    x ? local.max.x : local.min.x,
                    y ? local.max.y : local.min.y,
                    z ? local.max.z : local.min.z
                );

                glm::vec3 modelCorner =
                    glm::vec3(nodeWorld * glm::vec4(corner, 1.0f));

                result.expand(modelCorner);
            }
        }
    }

    return result;
}

void Model::selectModel() {
    for (auto& sm : m_meshes) {
        sm.isSelected = true;
    }
}
void Model::clearSelection() {
    for (auto& sm : m_meshes) {
        sm.isSelected = false;
    }
}