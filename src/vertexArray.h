#ifndef VAO_H
#define VAO_H

#include <vector>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "elementBuffer.h"
#include "vertexBuffer.h"


class VAO {
private:
	unsigned int m_vaoID;
	unsigned int m_stride;
public:
	VAO();
	~VAO();
	VAO(const VAO&) = delete;

	VAO& operator=(const VAO&) = delete;

	VAO(VAO&& other) noexcept { this->m_vaoID = other.m_vaoID; other.m_vaoID = 0; };

	VAO& operator=(VAO&& other) noexcept {
		if (this != &other) {
			if (m_vaoID) glDeleteVertexArrays(1, &m_vaoID);
			this->m_vaoID = other.m_vaoID;
			other.m_vaoID = 0;
		}
		return *this;
	};


	void bind();
	void unbind();
	void build(VBO& vbo, EBO& ebo, const std::vector<unsigned int>& attributes);
	void build(VBO& vbo, const std::vector<unsigned int>& attributes);
	unsigned int getStride() { return m_stride; }
};
#endif