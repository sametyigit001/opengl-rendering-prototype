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
	void loadModel(const std::string& path);
	int pick(float mouseX, float mouseY);
	void deleteModel() { m_model = nullptr; };
	int updateMeshAABB(int meshIndex);
	ray toModelLocalRay(ray worldRay);
	Model* getModel() { return m_model.get(); };
private:
	uint32_t m_width;
	uint32_t m_height;
	bool m_isSelected = false;

	std::unique_ptr<Camera> m_cam;
	std::unique_ptr<Shader> m_shader;
	std::unique_ptr<Model> m_model;
	
	lightManager m_lightMan;
};

#endif