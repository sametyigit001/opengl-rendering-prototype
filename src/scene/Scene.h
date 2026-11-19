#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "glad/glad.h"
#include "glm/glm.hpp"

#include "Camera.h"
#include "render/Shader.h"
#include "model/Model.h"
#include "Lights.h"
#include "spatial/Ray.h"
#include "Entity.h"

struct Entity;

struct storeIndexes {
	bool selected = false;
	int entityIndex = -1;
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
	int loadModel(const std::string& path, int index = -1);
	void selectNode(int entityIndex,int nodeIndex);
	bool setParent(int childIndex, int parentIndex);
	storeIndexes pick(float mouseX, float mouseY);
	int newEntity();
	void selectEntity(int entityId);
	void setEditorCameraSettings(float fov, float speed, float nearPlane, float farPlane);
	void setSelectedCameraSettings(int index,float fov, float speed, float nearPlane, float farPlane);
	void deleteModel(int entityIndex) {
		Entity* entity = getEntity(entityIndex);
		if (!entity || !entity->model) return;
		entity->model.reset();
		if (m_selectedEntity == entityIndex) m_selectedEntity = -1;
	};
	void deleteEntity(int entityIndex);
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
	void updateCamPos(int id);
	int getCurrentCam() { return currentCameraIndex; }
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

	Camera* getViewportCamera() {
		return m_cam.get();
	}

	int addCamera(int entityId);
	int addLight(int entityId);
	void deleteCamera(int entityId);
	void setCameraPos(int entityId, glm::vec3 pos);
	void setCameraRotation(int entityId, glm::vec3 rotation);
	void updateEntityMat(int entityId);
	void switchCamera(int entityId);
	void switchToEditor();
	void applyLight(light& light,int i,Shader& shader);
	void deleteLight(Entity* entity);
	void editLight();
private:
	uint32_t m_width;
	uint32_t m_height;
	bool m_isSelected = false;
	int m_selectedEntity{ -1 };

	std::unique_ptr<Camera> m_cam;
	std::unique_ptr<Shader> m_shader;
	std::vector<std::unique_ptr<Entity>> m_entity;
	std::unique_ptr<light> m_editorLight = std::make_unique<light>();
	Camera* m_activeCam = nullptr;
	int currentCameraIndex = -1;
	int pickedEntity;
	lightManager lightMan;

};

#endif
