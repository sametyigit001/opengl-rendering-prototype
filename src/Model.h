#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <unordered_map>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Mesh.h"

struct subMesh {
	mesh geometry;
	std::shared_ptr<Material> material;
};

class Model {
public:
	Model(const std::string& path);
	void loadModel(std::string path);
	void processNode(aiNode* node,const aiScene* scene);
	void draw(Shader& shader);
	subMesh processMesh(aiMesh* mesh, const aiScene* scene);
private:
	std::vector<subMesh> m_meshes;
	std::string directory;
	std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type);
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_loadedTextures;
};

#endif // !MODEL_H
