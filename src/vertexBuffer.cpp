#include "vertexBuffer.h"

VBO::VBO(unsigned int size, const void* data) {
	glGenBuffers(1, &m_vboID);
	glBindBuffer(GL_ARRAY_BUFFER, m_vboID);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

VBO::~VBO() {
	glDeleteBuffers(1,&m_vboID);
}

void VBO::bind() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vboID);
}
void VBO::unbind() {
	glBindBuffer(GL_ARRAY_BUFFER,0);
}