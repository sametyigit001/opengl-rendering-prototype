#include "Lights.h"
#include "Shader.h"

size_t lightManager::addLights(const lightConfig& light) {
    m_Lights.push_back(light);
    return m_Lights.size() - 1;
}

void lightManager::applyLights(const Shader& shaders) const {
    shaders.setInt("usedLight", static_cast<int>(m_Lights.size()));

    for (size_t i = 0; i < m_Lights.size(); i++) {
        shaders.setLight(m_Lights[i], static_cast<int>(i));
    }
}

void lightManager::updateLights(size_t index, const glm::vec3& position, const glm::vec3& direction) {
    if (index < m_Lights.size()) {
        m_Lights[index].direction = direction;
        m_Lights[index].position = position;
    }
}