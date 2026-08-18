#ifndef EBO_H
#define EBO_H
#include <vector>
#include "glad/glad.h"
#include "GLFW/glfw3.h"

class EBO {
private:
	unsigned int m_eboID;

public:
	EBO();
	~EBO();
	EBO(const EBO&) = delete;

	EBO& operator=(const EBO&) = delete;

	EBO(EBO&& other) noexcept { this->m_eboID = other.m_eboID; other.m_eboID = 0; };

	EBO& operator=(EBO&& other) noexcept {
		if (this != &other) {
			if (m_eboID) glDeleteBuffers(1, &m_eboID);
			this->m_eboID = other.m_eboID;
			other.m_eboID = 0;
		}
		return *this;
	};

	void bufferData(unsigned int size, const void* data);

	template <typename T>
	void bufferData(const std::vector<T>& data) {
		bufferData(static_cast<unsigned int>(data.size() * sizeof(T)), data.data());
	};

	void bind();
	void unbind();
};

#endif
