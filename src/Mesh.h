#ifndef MESH_H
#define MESH_H

#include "vertexArray.h"
#include "vertexBuffer.h"
#include "elementBuffer.h"

class mesh {
private:
	VAO m_vao;
	VBO m_vbo;
	EBO m_ebo;	

	int m_vertexCount;

public:
	mesh();
	~mesh();
};

#endif // !MESH_H
