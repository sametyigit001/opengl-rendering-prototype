#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <cstdint>

#include "glad/glad.h"
#include "glm/glm.hpp"

#include "Camera.h"
#include "render/Shader.h"
#include "model/Model.h"
#include "Lights.h"
#include "spatial/Ray.h"

struct Entity {
	int entityID;
	std::unique_ptr<Model> model = nullptr;
};

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
	void selectNode(int entityIndex,int nodeIndex);
	storeIndexes pick(float mouseX, float mouseY);
	int newEntity();
	void deleteModel(int entityIndex) {
		Entity* entity = getEntity(entityIndex);
		if (!entity || !entity->model) return;
		entity->model.reset();
		if (m_selectedEntity == entityIndex) m_selectedEntity = -1;
	};
	void deleteEntity(int entityIndex) {
		if (!getEntity(entityIndex)) return;
		m_entity[entityIndex].reset();
		if (m_selectedEntity == entityIndex) m_selectedEntity = -1;
	};
	int updateMeshAABB(int meshIndex);
	void selectModel(int entityIndex);
	ray toModelLocalRay(ray worldRay,const glm::mat4& modelMat);
	Model* getModel(int entityIndex) {
		if (entityIndex < 0 ||
			static_cast<size_t>(entityIndex) >= m_entity.size()) {
			return nullptr;
		}

		const auto& entity = m_entity[entityIndex];
		return entity ? entity->model.get() : nullptr;
	};
	void clearSelection() {
		for (auto& m : m_entity) {
			if (!m||!m->model) { continue; }
			m->model->deselectMesh();
		}
		m_selectedEntity = -1;
	}

	Entity* getEntity(int entityIndex)const{
		if (entityIndex < 0 ||
			static_cast<size_t>(entityIndex) >= m_entity.size()) {
			return nullptr;
		}
		return m_entity[entityIndex].get();
	}
private:
	uint32_t m_width;
	uint32_t m_height;
	bool m_isSelected = false;
	int m_selectedEntity{ -1 };

	std::unique_ptr<Camera> m_cam;
	std::unique_ptr<Shader> m_shader;
	std::vector<std::unique_ptr<Entity>> m_entity;
	int pickedModel;
	lightManager m_lightMan;
};

#endif
