#ifndef ENTITY_H
#define ENTITY_H

#include <memory>

#include "glad/glad.h"
#include "glm/gtc/matrix_transform.hpp"
#include "model/Model.h"


struct Transform {
	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };
};

struct Entity {
	int parentID = -1;
	int entityID = -1;
	Transform local;
	glm::mat4 worldMatrix = glm::mat4(1.0f);
	std::unique_ptr<Model> model = nullptr;
	std::vector<int> childIndices;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<light> light;

	glm::mat4 getLocalMatrix() const {
		glm::mat4 matrix(1.0f);
		matrix = glm::translate(matrix, local.position);
		matrix = glm::rotate(matrix, glm::radians(local.rotation.x), { 1.0f, 0.0f, 0.0f });
		matrix = glm::rotate(matrix, glm::radians(local.rotation.y), { 0.0f, 1.0f, 0.0f });
		matrix = glm::rotate(matrix, glm::radians(local.rotation.z), { 0.0f, 0.0f, 1.0f });
		return glm::scale(matrix, local.scale);
	}

	void draw(Shader& shader) {
		if (!model) return;
		glPolygonMode(GL_FRONT_AND_BACK, model->enableWireframe ? GL_LINE : GL_FILL);
		model->draw(ModelDrawContext{ shader, worldMatrix });
	}
};


#endif // !ENTITY_H
