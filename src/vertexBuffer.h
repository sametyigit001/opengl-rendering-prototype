#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H
#include <vector>
#include "glad/glad.h"
#include"GLFW/glfw3.h"

class VBO {
private:
	unsigned int m_vboID;

public:
	VBO();
	~VBO();
	VBO(const VBO&) = delete;

	VBO& operator=(const VBO&) = delete;

	VBO(VBO&& other) noexcept { this->m_vboID = other.m_vboID; other.m_vboID = 0; };

	VBO& operator=(VBO&& other) noexcept {
		if (this !=	 &other) {
			if (m_vboID) glDeleteBuffers(1, &m_vboID);
			this->m_vboID = other.m_vboID;
			other.m_vboID = 0;
		}
		return *this;
	};

	void bufferData(unsigned int size, const void* data);

	template <typename T>
	void bufferData(const std::vector<T>& data) {
		bufferData(static_cast<unsigned int>(data.size() * sizeof(T)), data.data());
	}

	void bind();
	void unbind();
};

#endif
