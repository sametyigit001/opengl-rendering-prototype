#ifndef EBO_H
#define EBO_H
#include "glad/glad.h"
#include "GLFW/glfw3.h"

class EBO {
private:
	unsigned int m_eboID;

public:
	EBO(unsigned int size, const unsigned int* data);
	~EBO();
	
	void bind();
	void unbind();
};

#endif
