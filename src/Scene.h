#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <cstdint>

#include "glad/glad.h"
#include "glm/glm.hpp"

#include "Camera.h"
#include "Shader.h"
#include "Model.h"
#include "Lights.h"
#include "Ray.h"

struct storeIndexes {
	bool selected = false;
	int modelIndex = -1;
	int nodeIndex = -1;
};
class Scene {
public:
	Scene(uint32_t width,uint32_t height);
	~Scene() = default;

	void resize(uint32_t width,uint32_t height);
	void update(float deltatime);
	void render();
	void onKeyboardInput(int direction, float deltatime);
	void onMouseMove(float xOffset, float yOffset);
	void onMouseScroll(float offset);
	int loadModel(const std::string& path);
	storeIndexes pick(float mouseX, float mouseY);
	void deleteModel(int modelIndex) { m_model[modelIndex] = nullptr; };
	int updateMeshAABB(int meshIndex);
	void selectModel(int modelIndex);
	ray toModelLocalRay(ray worldRay,const glm::mat4& modelMat);
	Model* getModel(int modelIndex) {
		if (modelIndex < 0 ||
			static_cast<size_t>(modelIndex) >= m_model.size()) {
			return nullptr;
		}

		return m_model[modelIndex].get();
	};
	void clearSelection() {
		for (auto& m : m_model) {
			if (m == nullptr) { continue; }
			m->deselectMesh();
		}
	}
private:
	uint32_t m_width;
	uint32_t m_height;
	bool m_isSelected = false;

	std::unique_ptr<Camera> m_cam;
	std::unique_ptr<Shader> m_shader;
	std::vector<std::unique_ptr<Model>> m_model;
	int pickedModel;
	lightManager m_lightMan;
};

#endif