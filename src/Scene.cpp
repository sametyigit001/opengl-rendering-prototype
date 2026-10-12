#include "Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>

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
	const auto shaderDir = std::filesystem::path(__FILE__).parent_path() / "shaders";
	const auto vertexPath = (shaderDir / "shader.vert").string();
	const auto fragmentPath = (shaderDir / "shader.frag").string();
	m_shader = std::make_unique<Shader>(vertexPath.c_str(), fragmentPath.c_str());
	for (auto& m : m_model) {
		m = nullptr;
	}
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
	if (!m_shader) return;

	// These values are shared by every model in this frame.
	m_shader->use();
	m_shader->setUniformView(m_cam->getViewMatrix());
	m_shader->setUniformProjection(m_cam->getProjectionMatrix());
	m_lightMan.applyLights(*m_shader);

	for (const auto& m : m_model) {
		if (!m) continue;
		else {
			if (m->enableWireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
			else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			m->draw(*m_shader);
		}
	}
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

int Scene::loadModel(const std::string& path) {
	std::cout << "\n[Engine] Attempting to load model at: " << path << "\n";
	int currentIndex{-1};
	try {
		m_model.push_back(std::make_unique<Model>(path));
		currentIndex = m_model.size() - 1;
		m_model[currentIndex].get()->setIndex(currentIndex);

		if (m_model[currentIndex]) {
			std::cout << "core:model loaded node count: " << m_model[currentIndex]->getNodes().size() << "\n";
		}
		return currentIndex;
	}
	catch (...) {
		std::cout << "core:error during Scene::modelLoad\n";
		m_model[currentIndex] = nullptr;
	}
}

storeIndexes Scene::pick(float mouseX, float mouseY) {
	bool hasModel = false;
	for(const auto& m :m_model)
	{
		if (m != nullptr) {
			hasModel = true;
			break;
		}
		hasModel = false;
		continue;
	}
	if (!hasModel) return storeIndexes{false};
	std::cout << "\n--- NEW CLICK ---\n";
	std::cout << "[RayGen] Mouse: (" << mouseX << ", " << mouseY
		<< ") | Screen: (" << m_width << ", " << m_height << ")\n";

	camDefaults camConf;
	camConf.position = m_cam->getPos();
	camConf.projection = m_cam->getProjectionMatrix();
	camConf.view = m_cam->getViewMatrix();

	ray worldRay = ray::rayGen(mouseX, mouseY, static_cast<float>(m_width), static_cast<float>(m_height), camConf);
	int meshIndex;
	
	for (size_t i = 0;i < m_model.size();i++) {
		Model* model = m_model[i].get();

		if (!model) continue;

		
		ray local = toModelLocalRay(worldRay,model->getModelMatrix());
		float outdist = 0.0f;
		meshIndex = model->pickSubMeshes(local, outdist);

		auto& nodes = model->getNodes();

		if (meshIndex == -1) {
			continue;
		}

		for (int nodeIndex = 0;nodeIndex < nodes.size();nodeIndex++) {
			for (int ownedMeshIndex : nodes[nodeIndex].meshIndices) {
				if (ownedMeshIndex == meshIndex) {

					storeIndexes id;
					id.modelIndex = static_cast<int>(i);
					id.nodeIndex = static_cast<int>(nodeIndex);
					id.selected = true;
					return id;
				}
			}
		}
	}
	return storeIndexes{false};
};


ray Scene::toModelLocalRay(ray worldRay,const glm::mat4& modelMat) {
	
	glm::mat4 invModel = glm::inverse(modelMat);
	ray local;
	local.origin = glm::vec3(invModel * glm::vec4(worldRay.origin, 1.0f));
	local.direction = glm::normalize(glm::vec3(invModel * glm::vec4(worldRay.direction, 0.0f)));

	return local;

	
}
void Scene::selectModel(int modelIndex) {
	const auto& model = m_model[modelIndex].get();
	model->selectModel();


}
