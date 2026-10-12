#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <cstdint>
#include <unordered_map>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"
#include "spatial/AABB.h"

struct triangleBvhNode {
	AABB bounds;
	std::uint32_t first = 0;  // Leaf: first triangle-order entry; internal: left child node.
	std::uint32_t second = 0; // Internal: right child node.
	std::uint16_t triangleCount = 0; // Zero for internal nodes.
};

struct subMesh {
	mesh geometry;
	std::shared_ptr<Material> material;
	AABB localAABB;
	bool isSelected = false;
	AABB modelAABB;
	std::vector<std::uint32_t> triangleOrder;
	std::vector<triangleBvhNode> triangleBvh;
	std::uint32_t triangleBvhRoot = 0;
};

struct modelNode {
	std::string name;
	int parentIndex = -1;
	std::vector<int> childIndices;
	std::vector<int> meshIndices;

	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };
	
};

class Model {
public:
	bool enableWireframe = false;
	const AABB& getAABB() const { return m_AABB; }
	const glm::mat4& getNorm() const { return normMat; }
	Model(const std::string& path);
	void loadModel(std::string path);
	int processNode(aiNode* node,const aiScene* scene, int parentIndex);
	void buildTriangleBvh(subMesh& submesh);
	void draw(Shader& shader);
	void selectNode(int index);
	int pickSubMeshes(const ray& localray, float& outdist);
	subMesh processMesh(aiMesh* mesh, const aiScene* scene);
	AABB transformAABB(const AABB& local, const glm::mat4& nodeWorld);
	void selectModel();
	void clearSelection();

	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };
	bool isChanged = true;

	glm::mat4 getModelMatrix() const {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		model = glm::scale(model, scale);
		return model * normMat;
	}

	const std::vector<modelNode>& getNodes()const { return m_nodes; }
	void deselectMesh() {
		for (auto& m : m_meshes) {
			m.isSelected = false;
		}
	}
	bool setNodePos(int index, float x, float y, float z) {
		if (index < 0 || static_cast<size_t>(index) >= m_nodes.size()) {
			return false;
		}
		m_nodes[index].position = { x,y,z };
		isChanged = true;
		return true;
	};
	bool setNodeRotation(int index, float x, float y, float z) {
		if (index < 0 || static_cast<size_t>(index) >= m_nodes.size()) {
			return false;
		}
		m_nodes[index].rotation = { x,y,z };
		isChanged = true;
		return true;
	};
	bool setNodeScale(int index, float x, float y, float z) {
		if (index < 0 || static_cast<size_t>(index) >= m_nodes.size()) {
			return false;
		}
		m_nodes[index].scale = { x,y,z };
		isChanged = true;
		return true;
	};
	void setIndex(int index) {
		m_index = index;
	};
private:
	AABB m_AABB;
	AABB m_pickAABB;
	glm::mat4 normMat{1.0f};
	std::vector<subMesh> m_meshes;
	std::vector<glm::mat4> m_nodeWorld;
	std::vector<glm::mat4> m_nodeWorldInverse;
	std::vector<unsigned char> m_nodeWorldInverseValid;
	std::vector<int> m_meshNodeIndex;
	std::string directory;
	std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type,const aiScene* scene);
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_loadedTextures;
	std::vector<modelNode> m_nodes;
	int m_index;
	void updateNodeTransformCache();
};

#endif // !MODEL_H
