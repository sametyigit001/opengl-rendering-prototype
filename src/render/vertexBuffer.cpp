#include "vertexBuffer.h"

VBO::VBO() {
	glGenBuffers(1, &m_vboID);
}

void VBO::bufferData(unsigned int size, const void* data) {
	this->bind();
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