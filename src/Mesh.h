#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include "vertexArray.h"
#include "vertexBuffer.h"
#include "elementBuffer.h"
#include "Texture.h"
#include "glm/glm.hpp"
#include "Shader.h"
#include "Material.h"


struct meshVertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};



class mesh {
private:
	VAO m_vao;
	VBO m_vbo;
	EBO m_ebo;

	std::vector<meshVertex> m_meshVertices;
	std::vector<unsigned int> m_meshIndices;


	GLsizei m_vertexCount{ 0 };
	GLsizei m_indexCount{ 0 };

	void setupMesh();

public:
	const std::vector<meshVertex>& getVertices()const { return m_meshVertices; }
	const std::vector<unsigned int>& getIndices()const { return m_meshIndices; }


	mesh(std::vector<meshVertex> vertices, std::vector<unsigned int> indices);
	mesh(std::vector<meshVertex> vertices);
	void Draw(const Shader& shader,const Material& material);

};

#endif // !MESH_H
