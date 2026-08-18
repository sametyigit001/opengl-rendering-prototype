#include "Mesh.h"

mesh::mesh(std::vector<meshVertex> vertices, std::vector<unsigned int> indices) {
	this->m_meshVertices = std::move(vertices);
	this->m_meshIndices = std::move(indices);

	m_vertexCount = static_cast<GLsizei>(m_meshVertices.size());
	m_indexCount = static_cast<GLsizei>(m_meshIndices.size());
	setupMesh();

}
mesh::mesh(std::vector<meshVertex> vertices) {
	this->m_meshVertices = vertices;

	m_vertexCount = static_cast<GLsizei>(vertices.size());
	m_indexCount =  0 ;
	setupMesh();
}

void mesh::Draw(const Shader& shader,const Material& material) {
	m_vao.bind();
	material.bind(shader);
	if(m_indexCount > 0) {
		glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
	}
	else {
		glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
	}
	m_vao.unbind();
}

void mesh::setupMesh() {
	m_vbo.bufferData(this->m_meshVertices);
	m_ebo.bufferData(this->m_meshIndices);
	m_vao.build(m_vbo,m_ebo,{3,3,2});

}