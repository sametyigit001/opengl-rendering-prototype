#include "Material.h"

Material::Material(std::shared_ptr<Texture> diffuse,
    std::shared_ptr<Texture> specular,
    float shininess,
    glm::vec3 specularColor)
{
    m_diffuseMap = std::move(diffuse);
    m_specularMap = std::move(specular);
    m_shininess = shininess;
    m_specularColor = specularColor;
}

Material::~Material() {}

void Material::bind(const Shader& shader) const {
    if (m_diffuseMap) {
        m_diffuseMap->bind(TextureSlot::Diffuse);
    }
    else {
        fallbackDiff();
    }
    shader.setInt("material.diffuse", TextureSlot::Diffuse);

    if (m_specularMap) {
        m_specularMap->bind(TextureSlot::Specular);
    }
    else {
        fallbackSpec();
    }
    shader.setInt("material.specular", TextureSlot::Specular);

    shader.setVec3("material.specularColor", m_specularColor);
    shader.setFloat("material.shininess", m_shininess);
}

void Material::fallbackDiff() const {
    Texture::getFallbackWhite()->bind(TextureSlot::Diffuse);
}

void Material::fallbackSpec() const {
    Texture::getFallbackWhite()->bind(TextureSlot::Specular);
}
