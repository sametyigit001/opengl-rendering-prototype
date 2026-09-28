#include "Scene.h"
#include <glm/gtc/matrix_transform.hpp>

Scene::Scene(uint32_t width, uint32_t height) : m_height(height), m_width(width) {
	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	cameraConfig camConf;
	camConf.windowHeight = static_cast<float>(m_height);
	camConf.windowWidth = static_cast<float>(m_width);
	
	lightConfig sun{};
	sun.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
	sun.ambient = glm::vec3(0.25f);
	sun.diffuse = glm::vec3(0.85f);
	sun.specular = glm::vec3(0.5f);
	m_lightMan.addLights(sun);
	m_cam = std::make_unique<Camera>(camConf);
	m_cam->setSpeed(10.0f);
	m_cam->setAspect(width,height);
	m_shader = std::make_unique<Shader>("C:/Users/Samet/source/repos/refactored_engine_code/src/shaders/shader.vert","C:/Users/Samet/source/repos/refactored_engine_code/src/shaders/shader.frag");
	m_model = nullptr;
}

void Scene::resize(uint32_t width, uint32_t height) {
	m_height = height;
	m_width = width;
	glViewport(0, 0, m_width, m_height);

	if (m_cam) {
		m_cam->setAspect(static_cast<float>(m_width), static_cast<float>(m_height));
	}
}

void Scene::render() {
	glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	if (!m_shader || !m_model) return;

	m_shader->use();

	m_shader->setUniformView(m_cam->getViewMatrix());
	m_shader->setUniformProjection(m_cam->getProjectionMatrix());
	m_shader->setUniformModel(m_model->getModelMatrix());
	

	m_lightMan.applyLights(*m_shader);

	m_model->draw(*m_shader);
}
void Scene::onKeyboardInput(int direction,float deltatime)
{
	switch (static_cast<cameraMovement>(direction)) {
	case cameraMovement::forward: m_cam->moveForward(deltatime); break;
	case cameraMovement::backward: m_cam->moveBackward(deltatime); break;
	case cameraMovement::right: m_cam->moveRight(deltatime); break;
	case cameraMovement::left: m_cam->moveLeft(deltatime); break;
	case cameraMovement::up: m_cam->moveUp(deltatime); break;
	case cameraMovement::down: m_cam->moveDown(deltatime); break;
	}
}

void Scene::onMouseMove(float xoffset, float yoffset) {
	m_cam->mouse_callback(xoffset, yoffset);
}

void Scene::loadModel(const std::string& path) {
	std::cout << "\n[Engine] Attempting to load model at: " << path << "\n";
	try {
		m_model = std::make_unique<Model>(path);

		if (m_model) {
			std::cout << "[Engine] Model loaded! Node count: " << m_model->getNodes().size() << "\n";
		}
	}
	catch (...) {
		std::cout << "[Engine] FATAL ERROR: Exception thrown during model load!\n";
		m_model = nullptr;
	}
}

bool Scene::pick(float mouseX, float mouseY) {
	if (!m_model) return false;

	std::cout << "\n--- NEW CLICK ---\n";
	std::cout << "[RayGen] Mouse: (" << mouseX << ", " << mouseY
		<< ") | Screen: (" << m_width << ", " << m_height << ")\n";

	camDefaults camConf;
	camConf.position = m_cam->getPos();
	camConf.projection = m_cam->getProjectionMatrix();
	camConf.view = m_cam->getViewMatrix();

	ray worldRay = ray::rayGen(mouseX, mouseY, static_cast<float>(m_width), static_cast<float>(m_height), camConf);
	glm::mat4 invModel = glm::inverse(m_model->getModelMatrix());

	ray local;
	local.origin = glm::vec3(invModel * glm::vec4(worldRay.origin, 1.0f));
	local.direction = glm::normalize(glm::vec3(invModel * glm::vec4(worldRay.direction, 0.0f)));

	float outdist = 0.0f;
	bool hit = m_model->pickSubMeshes(local, outdist);

	if (hit) {
		std::cout << ">>> FINAL RESULT: HIT at dist " << outdist << " <<<\n";
	}
	else {
		std::cout << ">>> FINAL RESULT: MISS <<<\n";
	}
	return hit;
};