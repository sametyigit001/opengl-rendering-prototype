#define GLM_ENABLE_EXPERIMENTAL
#include "Model.h"
#include "spatial/Ray.h"
#include "glm/gtx/quaternion.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
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

    // Build node transforms and mesh bounds once. Node setters invalidate this cache.
    isChanged = true;
    updateNodeTransformCache();
    for (const subMesh& mesh : m_meshes) {
        m_AABB.expand(mesh.modelAABB);
    }

    float targetSize = 5.0f;
    float maxDim = m_AABB.getMaxExtent();
    float scaleFactor = (maxDim > 0.0001f) ? (targetSize / maxDim) : 1.0f;
    glm::vec3 center = m_AABB.getCenter();

    
    normMat = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
    normMat = glm::translate(normMat, -center);
}

void Model::updateNodeTransformCache() {
    if (!isChanged &&
        m_nodeWorld.size() == m_nodes.size() &&
        m_nodeWorldInverse.size() == m_nodes.size() &&
        m_nodeWorldInverseValid.size() == m_nodes.size() &&
        m_meshNodeIndex.size() == m_meshes.size()) {
        return;
    }

    m_nodeWorld.assign(m_nodes.size(), glm::mat4(1.0f));
    m_nodeWorldInverse.assign(m_nodes.size(), glm::mat4(1.0f));
    m_nodeWorldInverseValid.assign(m_nodes.size(), 0);
    m_meshNodeIndex.assign(m_meshes.size(), -1);
    for (subMesh& mesh : m_meshes) {
        mesh.modelAABB = transformAABB(mesh.localAABB, glm::mat4(1.0f));
    }

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const modelNode& node = m_nodes[i];

        glm::mat4 local(1.0f);
        local = glm::translate(local, node.position);
        local = glm::rotate(local, glm::radians(node.rotation.x), { 1.0f, 0.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.y), { 0.0f, 1.0f, 0.0f });
        local = glm::rotate(local, glm::radians(node.rotation.z), { 0.0f, 0.0f, 1.0f });
        local = glm::scale(local, node.scale);

        const bool hasValidParent = node.parentIndex >= 0 &&
            static_cast<size_t>(node.parentIndex) < i;
        m_nodeWorld[i] = hasValidParent
            ? m_nodeWorld[node.parentIndex] * local
            : local;

        const float determinant = glm::determinant(m_nodeWorld[i]);
        if (std::isfinite(determinant) && std::abs(determinant) > 1e-8f) {
            m_nodeWorldInverse[i] = glm::inverse(m_nodeWorld[i]);
            m_nodeWorldInverseValid[i] = 1;
        }

        for (int meshIndex : node.meshIndices) {
            if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= m_meshes.size())
                continue;

            m_meshNodeIndex[meshIndex] = static_cast<int>(i);
            m_meshes[meshIndex].modelAABB =
                transformAABB(m_meshes[meshIndex].localAABB, m_nodeWorld[i]);
        }
    }

    m_pickAABB.reset();
    for (const subMesh& mesh : m_meshes)
        m_pickAABB.expand(mesh.modelAABB);

    isChanged = false;
}

void Model::draw(const ModelDrawContext& context) {
    updateNodeTransformCache();
    const glm::mat4 modelWorld = context.entityMatrix * getModelLocalMatrix();
    Shader& shader = context.shader;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const modelNode& node = m_nodes[i];
        const glm::mat4& nodeWorld = m_nodeWorld[i];

        for (int meshIndex : node.meshIndices) {
            subMesh& submesh = m_meshes[meshIndex];
            shader.setUniformModel(modelWorld * nodeWorld);
            shader.setInt("u_isSelected", submesh.isSelected ? 1 : 0);

            if (submesh.material) {
                submesh.geometry.Draw(shader, *submesh.material);
            }
        }
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
    subMesh processed{
        mesh(std::move(vertices), std::move(indices)),
        mat,
        submeshAABB,
        false
    };
    buildTriangleBvh(processed);
    return processed;
}

void Model::buildTriangleBvh(subMesh& submesh) {
    const auto& vertices = submesh.geometry.getVertices();
    const auto& indices = submesh.geometry.getIndices();
    const size_t triangleCount = indices.empty()
        ? vertices.size() / 3
        : indices.size() / 3;

    constexpr std::uint32_t leafSize = 32;
    if (triangleCount <= leafSize ||
        triangleCount >= (std::numeric_limits<std::uint32_t>::max)()) {
        return;
    }

    auto spreadBits = [](std::uint32_t value) {
        value &= 0x000003ffu;
        value = (value | (value << 16)) & 0x030000ffu;
        value = (value | (value << 8)) & 0x0300f00fu;
        value = (value | (value << 4)) & 0x030c30c3u;
        value = (value | (value << 2)) & 0x09249249u;
        return value;
    };
    auto quantize = [](float value, float minimum, float extent) {
        if (extent <= 1e-8f)
            return 0u;
        const float normalized = std::clamp((value - minimum) / extent, 0.0f, 0.999999f);
        return static_cast<std::uint32_t>(normalized * 1024.0f);
    };

    const glm::vec3 extent = submesh.localAABB.getExtent();
    submesh.triangleOrder.resize(triangleCount);
    std::vector<std::uint32_t> mortonCodes(triangleCount);

    for (std::uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const size_t offset = static_cast<size_t>(triangle) * 3;
        const std::uint32_t i0 = indices.empty() ? static_cast<std::uint32_t>(offset) : indices[offset];
        const std::uint32_t i1 = indices.empty() ? static_cast<std::uint32_t>(offset + 1) : indices[offset + 1];
        const std::uint32_t i2 = indices.empty() ? static_cast<std::uint32_t>(offset + 2) : indices[offset + 2];
        const glm::vec3 centroid =
            (vertices[i0].position + vertices[i1].position + vertices[i2].position) / 3.0f;

        const std::uint32_t x = quantize(centroid.x, submesh.localAABB.min.x, extent.x);
        const std::uint32_t y = quantize(centroid.y, submesh.localAABB.min.y, extent.y);
        const std::uint32_t z = quantize(centroid.z, submesh.localAABB.min.z, extent.z);
        mortonCodes[triangle] =
            spreadBits(x) | (spreadBits(y) << 1) | (spreadBits(z) << 2);
        submesh.triangleOrder[triangle] = triangle;
    }

    std::sort(submesh.triangleOrder.begin(), submesh.triangleOrder.end(),
        [&mortonCodes](std::uint32_t left, std::uint32_t right) {
            return mortonCodes[left] == mortonCodes[right]
                ? left < right
                : mortonCodes[left] < mortonCodes[right];
        });
    std::vector<std::uint32_t>().swap(mortonCodes);

    std::vector<std::uint32_t> currentLevel;
    currentLevel.reserve((triangleCount + leafSize - 1) / leafSize);
    for (std::uint32_t first = 0; first < triangleCount; first += leafSize) {
        triangleBvhNode leaf;
        leaf.first = first;
        leaf.triangleCount = static_cast<std::uint16_t>(
            std::min<std::size_t>(leafSize, triangleCount - first));

        const std::uint32_t end = first + leaf.triangleCount;
        for (std::uint32_t orderIndex = first; orderIndex < end; ++orderIndex) {
            const size_t triangleOffset =
                static_cast<size_t>(submesh.triangleOrder[orderIndex]) * 3;
            const std::uint32_t i0 = indices.empty()
                ? static_cast<std::uint32_t>(triangleOffset)
                : indices[triangleOffset];
            const std::uint32_t i1 = indices.empty()
                ? static_cast<std::uint32_t>(triangleOffset + 1)
                : indices[triangleOffset + 1];
            const std::uint32_t i2 = indices.empty()
                ? static_cast<std::uint32_t>(triangleOffset + 2)
                : indices[triangleOffset + 2];
            leaf.bounds.expand(vertices[i0].position);
            leaf.bounds.expand(vertices[i1].position);
            leaf.bounds.expand(vertices[i2].position);
        }
        leaf.bounds.min -= glm::vec3(0.005f);
        leaf.bounds.max += glm::vec3(0.005f);

        currentLevel.push_back(static_cast<std::uint32_t>(submesh.triangleBvh.size()));
        submesh.triangleBvh.push_back(leaf);
    }

    while (currentLevel.size() > 1) {
        std::vector<std::uint32_t> nextLevel;
        nextLevel.reserve((currentLevel.size() + 1) / 2);

        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            if (i + 1 == currentLevel.size()) {
                nextLevel.push_back(currentLevel[i]);
                continue;
            }

            triangleBvhNode parent;
            parent.first = currentLevel[i];
            parent.second = currentLevel[i + 1];
            parent.bounds.expand(submesh.triangleBvh[parent.first].bounds);
            parent.bounds.expand(submesh.triangleBvh[parent.second].bounds);

            nextLevel.push_back(static_cast<std::uint32_t>(submesh.triangleBvh.size()));
            submesh.triangleBvh.push_back(parent);
        }
        currentLevel.swap(nextLevel);
    }

    submesh.triangleBvhRoot = currentLevel.front();
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

int Model::pickSubMeshes(const ray& localRay, float& outDist) {
    updateNodeTransformCache();

    float modelEntryDist = 0.0f;
    if (!m_pickAABB.intersects(localRay, modelEntryDist))
        return -1;

    float closestDist = (std::numeric_limits<float>::max)();
    int hitMeshIndex = -1;

    int submeshHits = 0;
    int triHits = 0;

    for (size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex) {
        subMesh& sm = m_meshes[meshIndex];
        const int nodeIndex = m_meshNodeIndex[meshIndex];
        const bool hasNodeTransform = nodeIndex >= 0 &&
            static_cast<size_t>(nodeIndex) < m_nodeWorld.size();
        const bool hasValidInverse = hasNodeTransform &&
            m_nodeWorldInverseValid[nodeIndex] != 0;
        const glm::mat4 identity(1.0f);
        const glm::mat4& nodeMatrix = hasNodeTransform
            ? m_nodeWorld[nodeIndex]
            : identity;

        ray meshLocalRay = localRay;
        float rayParameterScale = 1.0f;
        bool canTransformRay = hasValidInverse;
        if (canTransformRay) {
            const glm::mat4& inverseNode = m_nodeWorldInverse[nodeIndex];
            meshLocalRay.origin = glm::vec3(
                inverseNode * glm::vec4(localRay.origin, 1.0f));
            const glm::vec3 transformedDirection = glm::vec3(
                inverseNode * glm::vec4(localRay.direction, 0.0f));
            rayParameterScale = glm::length(transformedDirection);
            if (rayParameterScale > 1e-8f) {
                meshLocalRay.direction = transformedDirection / rayParameterScale;
            }
            else {
                canTransformRay = false;
            }
        }

        float aabbDist = 0.0f;
        const AABB& broadPhaseBounds = canTransformRay ? sm.localAABB : sm.modelAABB;
        const ray& broadPhaseRay = canTransformRay ? meshLocalRay : localRay;
        if (!broadPhaseBounds.intersects(broadPhaseRay, aabbDist)) {
            continue;
        }
        if (canTransformRay)
            aabbDist /= rayParameterScale;
        if (aabbDist >= closestDist)
            continue;

        ++submeshHits;

        const auto& vertices = sm.geometry.getVertices();
        const auto& indices = sm.geometry.getIndices();

        auto testTriangle = [&](std::uint32_t triangleOrdinal) {
            const size_t triangleOffset = static_cast<size_t>(triangleOrdinal) * 3;
            const size_t i0 = indices.empty()
                ? triangleOffset
                : indices[triangleOffset];
            const size_t i1 = indices.empty()
                ? triangleOffset + 1
                : indices[triangleOffset + 1];
            const size_t i2 = indices.empty()
                ? triangleOffset + 2
                : indices[triangleOffset + 2];
            const glm::vec3 v0 = canTransformRay
                ? vertices[i0].position
                : glm::vec3(nodeMatrix * glm::vec4(vertices[i0].position, 1.0f));
            const glm::vec3 v1 = canTransformRay
                ? vertices[i1].position
                : glm::vec3(nodeMatrix * glm::vec4(vertices[i1].position, 1.0f));
            const glm::vec3 v2 = canTransformRay
                ? vertices[i2].position
                : glm::vec3(nodeMatrix * glm::vec4(vertices[i2].position, 1.0f));

            float triDist = 0.0f;
            if (intersectTriangle(broadPhaseRay, v0, v1, v2, triDist)) {
                ++triHits;
                const float modelRayDist = canTransformRay
                    ? triDist / rayParameterScale
                    : triDist;
                if (modelRayDist < closestDist) {
                    closestDist = modelRayDist;
                    hitMeshIndex = static_cast<int>(meshIndex);
                }
            }
        };

        if (canTransformRay && !sm.triangleBvh.empty()) {
            auto visitBvh = [&](auto&& visitBvh, std::uint32_t nodeIndex, float nodeEntryDist) -> void {
                if (nodeEntryDist / rayParameterScale >= closestDist)
                    return;

                const triangleBvhNode& node = sm.triangleBvh[nodeIndex];
                if (node.triangleCount > 0) {
                    const std::uint32_t end = node.first + node.triangleCount;
                    for (std::uint32_t orderIndex = node.first; orderIndex < end; ++orderIndex)
                        testTriangle(sm.triangleOrder[orderIndex]);
                    return;
                }

                const triangleBvhNode& left = sm.triangleBvh[node.first];
                const triangleBvhNode& right = sm.triangleBvh[node.second];
                float leftEntry = 0.0f;
                float rightEntry = 0.0f;
                const bool hitLeft = left.bounds.intersects(meshLocalRay, leftEntry);
                const bool hitRight = right.bounds.intersects(meshLocalRay, rightEntry);

                if (hitLeft && hitRight) {
                    if (leftEntry <= rightEntry) {
                        visitBvh(visitBvh, node.first, leftEntry);
                        visitBvh(visitBvh, node.second, rightEntry);
                    }
                    else {
                        visitBvh(visitBvh, node.second, rightEntry);
                        visitBvh(visitBvh, node.first, leftEntry);
                    }
                }
                else if (hitLeft) {
                    visitBvh(visitBvh, node.first, leftEntry);
                }
                else if (hitRight) {
                    visitBvh(visitBvh, node.second, rightEntry);
                }
            };

            float rootEntry = 0.0f;
            const triangleBvhNode& root = sm.triangleBvh[sm.triangleBvhRoot];
            if (root.bounds.intersects(meshLocalRay, rootEntry)) {
                visitBvh(visitBvh, sm.triangleBvhRoot, rootEntry);
            }
        }
        else {
            const size_t triangleCount = indices.empty()
                ? vertices.size() / 3
                : indices.size() / 3;
            for (std::uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
                testTriangle(triangle);
            }
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
