#include "Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <cmath>

Scene::Scene(uint32_t width, uint32_t height) : m_height(height), m_width(width) {
	glViewport(0, 0, width, height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	cameraConfig camConf;
	camConf.windowHeight = static_cast<float>(m_height);
	camConf.windowWidth = static_cast<float>(m_width);
	m_editorLight->config.type = lightTypes::directional;
	m_editorLight->config.direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
	m_cam = std::make_unique<Camera>(camConf);
	m_cam->setSpeed(10.0f);
	m_cam->setAspect(width,height);
	m_activeCam = m_cam.get();
	const auto shaderDir = std::filesystem::path(__FILE__).parent_path().parent_path() / "shaders";
	const auto vertexPath = (shaderDir / "shader.vert").string();
	const auto fragmentPath = (shaderDir / "shader.frag").string();
	m_shader = std::make_unique<Shader>(vertexPath.c_str(), fragmentPath.c_str());
}

void Scene::resize(uint32_t width, uint32_t height) {
	m_height = height;
	m_width = width;
	glViewport(0, 0, m_width, m_height);

	if (m_cam) {
		m_cam->setAspect(static_cast<float>(m_width), static_cast<float>(m_height));
	}
	for (auto& entity : m_entity) {
		if (entity && entity->camera) entity->camera->setAspect(static_cast<float>(m_width), static_cast<float>(m_height));
	}
}

void Scene::render() {
	glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	if (!m_shader) return;
	int counter = 1;

	m_shader->use();
	m_shader->setUniformView(m_activeCam->getViewMatrix());
	m_shader->setUniformProjection(m_activeCam->getProjectionMatrix());
	m_shader->setVec3("viewPos", m_activeCam->getPos());
	applyLight(*m_editorLight, 0, *m_shader);



	for (const auto& m : m_entity) {
		if (m && counter <= 15 && m->light) {
			applyLight(*m->light,counter,*m_shader);
			counter++;
		}
	}
	m_shader->setInt("usedLight", static_cast<int>(counter));
	for (const auto& m : m_entity) {

		if (m) m->draw(*m_shader);
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
	m_cam->syncToWorldMatrix(glm::mat4(1.0f));
}

void Scene::onMouseMove(float xoffset, float yoffset) {
	m_cam->mouse_callback(xoffset, yoffset);
	m_cam->syncToWorldMatrix(glm::mat4(1.0f));
}

int Scene::loadModel(const std::string& path, int index) {
	std::cout << "\n[Engine] Attempting to load model at: " << path << "\n";
	try {
		if (index == -1) {
			auto entity = std::make_unique<Entity>();
			entity->model = std::make_unique<Model>(path);

			const int entityIndex = static_cast<int>(m_entity.size());
			entity->entityID = entityIndex;
			entity->model->setEntityIndex(entityIndex);

			const auto nodeCount = entity->model->getNodes().size();
			m_entity.push_back(std::move(entity));

			std::cout << "core:model loaded node count: " << nodeCount << '\n';
			return entityIndex;
		}
		else {
			Entity* entity = getEntity(index);
			if (!entity || entity->model) return -1;
			entity->model = std::make_unique<Model>(path);
			entity->model->setEntityIndex(index);
			return index;
		}
	}
	catch (...) {
		std::cout << "core:error during Scene::modelLoad\n";
		return -1;
	}
}

storeIndexes Scene::pick(float mouseX, float mouseY) {
	bool hasModel = false;
	for(const auto& m :m_entity)
	{
		if (m && m->model) {
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
	camConf.position = m_activeCam->getPos();
	camConf.projection = m_activeCam->getProjectionMatrix();
	camConf.view = m_activeCam->getViewMatrix();

	ray worldRay = ray::rayGen(mouseX, mouseY, static_cast<float>(m_width), static_cast<float>(m_height), camConf);
	int meshIndex;
	
	for (size_t i = 0;i < m_entity.size();i++) {
		if (!m_entity[i]) continue;
		Model* model = m_entity[i]->model.get();

		if (!model) continue;

		
		ray local = toModelLocalRay(worldRay,
			m_entity[i]->worldMatrix * model->getModelLocalMatrix());
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
					id.entityIndex = static_cast<int>(i);
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
void Scene::selectModel(int entityIndex) {

	Model* model = getModel(entityIndex);
	if (!model)
	{
		return;
	}
	clearSelection();
	model->selectModel();
	m_selectedEntity = entityIndex;
}

void Scene::selectNode(int entityIndex,int nodeIndex)
{
	Model* model = getModel(entityIndex);
	if (!model)
	{
		return;
	}
	clearSelection();
	model->selectNode(nodeIndex);
	m_selectedEntity = entityIndex;
}

int Scene::newEntity() {
	auto entity = std::make_unique<Entity>();
	entity->entityID = m_entity.size();
	m_entity.push_back(std::move(entity));
	return m_entity.size() - 1;
}

bool Scene::setParent(int child, int parent)
{
	Entity* childEntity = getEntity(child);
	if (!childEntity || parent < -1 || child == parent)
		return false;

	Entity* newParent = parent == -1 ? nullptr : getEntity(parent);
	if (parent != -1 && !newParent)
		return false;

	int ancestor = parent;
	for (size_t steps = 0; ancestor != -1; ++steps)
	{
		if (steps >= m_entity.size() || ancestor == child)
			return false;

		Entity* current = getEntity(ancestor);
		if (!current)
			return false;

		ancestor = current->parentID;


	}

	const int oldParentIndex = childEntity->parentID;
	if (oldParentIndex == parent)
		return true;

	if (Entity* oldParent = getEntity(oldParentIndex))
	{
		auto& children = oldParent->childIndices;
		children.erase(
			std::remove(children.begin(), children.end(), child),
			children.end());
	}

	childEntity->parentID = parent;

	if (newParent)
		newParent->childIndices.push_back(child);
	updateEntityMat(child);
	return true;
}

void Scene::updateEntityMat(int entityId) {
	Entity* entity = getEntity(entityId);
	if (!entity) return;

	Entity* parent = getEntity(entity->parentID);

	glm::mat4 parentWorld = parent ? parent->worldMatrix : glm::mat4(1.0f);

	entity->worldMatrix = parentWorld * entity->getLocalMatrix();

	for (int child : entity->childIndices) {
		updateEntityMat(child);
	}
	updateCamPos(entityId);
}
void Scene::selectEntity(int entityId) {
	if (!getEntity(entityId)) return;

	clearSelection();
	std::vector<int> pending{ entityId };

	while (!pending.empty()) {
		int id = pending.back();
		pending.pop_back();

		Entity* entity = getEntity(id);
		if (!entity) continue;

		if (entity->model)
			entity->model->selectModel();

		for (int childId : entity->childIndices)
			pending.push_back(childId);
	}
	m_selectedEntity = entityId;
}
void Scene::setEditorCameraSettings(float fov,float speed,float nearPlane,float farPlane)
{
	if (!std::isfinite(fov) || !std::isfinite(speed) || !std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
		fov <= 0.0f || fov >= 180.0f || speed <= 0.0f || nearPlane <= 0.0f || farPlane <= nearPlane) return;
	m_cam->setFOV(fov);
	m_cam->setSpeed(speed);
	m_cam->setFarPlane(farPlane);
	m_cam->setNearPlane(nearPlane);
}
void Scene::setSelectedCameraSettings(int index,float fov,float speed,float nearPlane,float farPlane)
{
	Entity* entity = getEntity(index);
	if (!entity || !entity->camera) return;
	if (!std::isfinite(fov) || !std::isfinite(speed) || !std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
		fov <= 0.0f || fov >= 180.0f || speed <= 0.0f || nearPlane <= 0.0f || farPlane <= nearPlane) return;
	entity->camera->setFOV(fov);
	entity->camera->setSpeed(speed);
	entity->camera->setFarPlane(farPlane);
	entity->camera->setNearPlane(nearPlane);
}
int Scene::addCamera(int entityId) {
	Entity* entity = getEntity(entityId);
	if (!entity || entity->camera) return 0;
	cameraConfig config;
	config.windowWidth = m_width;
	config.windowHeight = m_height;
	entity->camera = std::make_unique<Camera>(config);
	updateCamPos(entityId);
	return 1;
}
int Scene::addLight(int entityId) {
	Entity* entity = getEntity(entityId);
	if (!entity || entity->light) return 0;
	entity->light = std::make_unique<light>();
	return 1;
}
void Scene::switchCamera(int entityId) {
	if (currentCameraIndex == entityId || !getEntity(entityId) || !getEntity(entityId)->camera) return;
	m_activeCam = getEntity(entityId)->camera.get();
	currentCameraIndex = entityId;
}
void Scene::switchToEditor() {
	m_activeCam = m_cam.get();
	currentCameraIndex = -1;
}
void Scene::updateCamPos(int id) {
	Entity* entity = getEntity(id);
	if (!entity || !entity->camera) return;

	entity->camera->syncToWorldMatrix(entity->worldMatrix);
}
void Scene::setCameraPos(int entityId, glm::vec3 pos) {
	Entity* entity = getEntity(entityId);
	if (!entity || !entity->camera || !std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) return;
	entity->camera->setCameraPos(pos);
	updateCamPos(entityId);
}
void Scene::setCameraRotation(int entityId, glm::vec3 rotation) {
	Entity* entity = getEntity(entityId);
	if (!entity || !entity->camera || !std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z)) return;
	entity->camera->setCameraRotation(rotation);
	updateCamPos(entityId);
}
void Scene::deleteCamera(int entityId) {
	Entity* entity = getEntity(entityId);
	if (!entity || !entity->camera) return;
	if (currentCameraIndex == entityId) switchToEditor();
	entity->camera.reset();
}

void Scene::deleteEntity(int entityIndex) {
	Entity* entity = getEntity(entityIndex);
	if (!entity) return;
	// Copy because deleting a child also removes it from this list.
	const auto children = entity->childIndices;
	for (int child : children) deleteEntity(child);
	if (Entity* parent = getEntity(entity->parentID)) {
		auto& siblings = parent->childIndices;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), entityIndex), siblings.end());
	}
	if (currentCameraIndex == entityIndex) switchToEditor();
	m_entity[entityIndex].reset();
	if (m_selectedEntity == entityIndex) m_selectedEntity = -1;
}
void Scene::applyLight(light& light,int i, Shader& shader) {
	shader.setLight(light, static_cast<int>(i));
}
void Scene::deleteLight(Entity* entity) {
	entity->light.reset();
}
