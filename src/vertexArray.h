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
	void bind();
	void unbind();
	void build(VBO& vbo, EBO& ebo, const std::vector<unsigned int>& attributes);
	void build(VBO& vbo, const std::vector<unsigned int>& attributes);
	unsigned int getStride() { return m_stride; }
};
#endif