#include "elementBuffer.h"

EBO::EBO(unsigned int size, const unsigned int* data) {
	glGenBuffers(1, &m_eboID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_eboID);
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