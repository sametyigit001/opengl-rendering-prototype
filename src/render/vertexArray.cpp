#include "vertexArray.h"

VAO::VAO() {
	glGenVertexArrays(1, &m_vaoID);
};
VAO::~VAO() {
	glDeleteVertexArrays(1, &m_vaoID);
}

void VAO::build(VBO& vbo, EBO& ebo, const std::vector<unsigned int>& attributes) {
	this->bind();
	vbo.bind();
	ebo.bind();

	unsigned int totalComp = 0;
	for (unsigned int count : attributes) {
		totalComp += count;
	}
	unsigned int stride = totalComp * sizeof(float);
	m_stride = stride;
	unsigned int byteOffset = 0;
	for (int i = 0;i < attributes.size();i++) {
		glVertexAttribPointer(
			i,
			attributes[i],
			GL_FLOAT,
			GL_FALSE,
			stride,
			(const void*)byteOffset
		);
		byteOffset += attributes[i] * sizeof(float);
		glEnableVertexAttribArray(static_cast<GLuint>(i));
	}
	this->unbind();
}
void VAO::build(VBO& vbo, const std::vector<unsigned int>& attributes) {
	this->bind();
	vbo.bind();

	unsigned int totalComp = 0;
	for (unsigned int count : attributes) {
		totalComp += count;
	}
	unsigned int stride = totalComp * sizeof(float);
	m_stride = stride;
	unsigned int byteOffset = 0;
	for (int i = 0;i < attributes.size();i++) {
		glVertexAttribPointer(
			i,
			attributes[i],
			GL_FLOAT,
			GL_FALSE,
			stride,
			(const void*)byteOffset
		);
		byteOffset += attributes[i] * sizeof(float);
		glEnableVertexAttribArray(static_cast<GLuint>(i));
	}
	this->unbind();
}

void VAO::bind() {
	glBindVertexArray(m_vaoID);
}

void VAO::unbind() {
	glBindVertexArray(0);
}