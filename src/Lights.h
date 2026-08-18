#ifndef LIGHTS_H
#define LIGHTS_H
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

class Shader;

enum class lightTypes : int {
	directional = 0,
	point = 1,
	spot = 2
};

struct lightConfig {
	lightTypes type = lightTypes::directional;
	glm::vec3 direction = glm::vec3(2.0f,-2.0f,1.0f);
	glm::vec3 position;

	glm::vec3 ambient = glm::vec3(0.05f);
	glm::vec3 diffuse = glm::vec3(1.0f);
	glm::vec3 specular = glm::vec3(1.0f);

	float cutOff = glm::cos(glm::radians(12.5f));
	float outerCutOff = glm::cos(glm::radians(17.5f));

	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;
};

class lightManager {
private:
	std::vector<lightConfig> m_Lights;
public:
	size_t addLights(const lightConfig& light);
	void applyLights(const Shader& shaders)const;
	int getSize() { return static_cast<int>(m_Lights.size()); }
	lightConfig& getLight(size_t index) { return m_Lights[index]; }
	void updateLights(size_t index, const glm::vec3& position,const glm::vec3& direction);
};


#endif // LIGHTS_H
