#ifndef MATERIAL_H
#define MATERIAL_H
#include <memory>
#include <vector>
#include "Texture.h"
#include "Shader.h"

class Material {
public:
    Material(std::shared_ptr<Texture> diffuse,
        std::shared_ptr<Texture> specular,
        float shininess = 32.0f,
        glm::vec3 specularColor = glm::vec3(0.5f));

    ~Material();

    void bind(const Shader& shader) const;

private:
    void fallbackDiff() const;
    void fallbackSpec() const;

    std::shared_ptr<Texture> m_diffuseMap;
    std::shared_ptr<Texture> m_specularMap;
    float m_shininess;
    glm::vec3 m_specularColor;
};

#endif // !MATERIAL_H
