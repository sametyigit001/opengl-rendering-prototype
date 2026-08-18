#ifndef TEXTURE_H
#define TEXTURE_H

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#include "STB/stb_image.h"
#include "glad/glad.h" 
#include "GLFW/glfw3.h" 
#include <iostream>
#include <memory>
#include <string>

namespace TextureSlot {
    constexpr unsigned int Diffuse = 0;
    constexpr unsigned int Specular = 1;
    constexpr unsigned int Normal = 2;
}

enum class texType {
    tex2d,
    tex3d
};

enum class wrapMode {
    repeat,
    clamp,
    border,
    mirror
};

struct textureConfig {
    wrapMode wrapS;
    wrapMode wrapT;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};

class Texture {
private:
    texType m_type;
    unsigned int m_textureID;
    int m_width;
    int m_height;
    int m_channels;
    int m_slot;

public:
    Texture(const char* texturePath, texType type, const textureConfig& config);
    Texture(const char* texturePath, texType type);
    Texture(const char* texturePath);
    Texture(const std::string& texturePath);
    Texture(const unsigned char* data, int width, int height, int channels);
    ~Texture();

    static std::shared_ptr<Texture> getFallbackWhite();
    static std::shared_ptr<Texture> getFallbackBlack();
    void bind(unsigned int slot = 0);
    void unbind() const;
};

#endif
