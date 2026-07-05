#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H
#include "glad/glad.h"
#include"GLFW/glfw3.h"

class VBO {
private:
	unsigned int m_vboID;

public:
	VBO(unsigned int size, const void* data);
	~VBO();

	void bind();
	void unbind();
};

#endif
