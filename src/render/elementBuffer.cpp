#include "elementBuffer.h"

EBO::EBO() {
	glGenBuffers(1, &m_eboID);
}

void EBO::bufferData(unsigned int size, const void* data) {
	this->bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}
EBO::~EBO() {
	glDeleteBuffers(1,&m_eboID);
}

void EBO::bind() {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_eboID);
}
void EBO::unbind() {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
