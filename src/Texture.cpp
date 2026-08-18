#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include <algorithm>

Texture::Texture(const char* path, texType type, const textureConfig& config)
    : m_type(type), m_slot(0), m_width(0), m_height(0), m_channels(0)
{
    glGenTextures(1, &m_textureID);

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path, &m_width, &m_height, &m_channels, 0);

    switch (m_type) {
    case(texType::tex2d): {
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        switch (config.wrapS) {
        case(wrapMode::repeat): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
        case(wrapMode::clamp):  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
        case(wrapMode::border): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); break;
        case(wrapMode::mirror): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT); break;
        }

        switch (config.wrapT) {
        case(wrapMode::repeat): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
        case(wrapMode::clamp):  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
        case(wrapMode::border): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER); break;
        case(wrapMode::mirror): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT); break;
        }

        if (config.wrapS == wrapMode::border || config.wrapT == wrapMode::border) {
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, config.borderColor);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        float maxAnisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        if (maxAnisotropy > 0.0f) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAnisotropy, 16.0f));
        }


        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.4f);
        if (data) {
            GLenum internalFormat = GL_RGB8;
            GLenum dataFormat = GL_RGB;

            if (m_channels == 1) { internalFormat = GL_R8;   dataFormat = GL_RED; }
            else if (m_channels == 3) { internalFormat = GL_RGB8;  dataFormat = GL_RGB; }
            else if (m_channels == 4) { internalFormat = GL_RGBA8; dataFormat = GL_RGBA; }

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else {
            std::cout << "Failed to load texture at path: " << path << std::endl;
        }
        stbi_image_free(data);
        break;
    }
    }
}

Texture::Texture(const unsigned char* data, int width, int height, int channels)
    : m_type(texType::tex2d), m_slot(0), m_width(width), m_height(height), m_channels(channels)
{
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLenum internalFormat = GL_RGBA8;
    GLenum dataFormat = GL_RGBA;

    if (channels == 1) {
        internalFormat = GL_R8;
        dataFormat = GL_RED;
    }
    else if (channels == 3) {
        internalFormat = GL_RGB8;
        dataFormat = GL_RGB;
    }
    else if (channels == 4) {
        internalFormat = GL_RGBA8;
        dataFormat = GL_RGBA;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(const char* path, texType type)
    : Texture(path, type, textureConfig{ wrapMode::repeat, wrapMode::repeat }) {
}

Texture::Texture(const char* path)
    : Texture(path, texType::tex2d, textureConfig{ wrapMode::repeat, wrapMode::repeat }) {
}


Texture::Texture(const std::string& path)
    : Texture(path.c_str()) {
}
Texture::~Texture() {
    glDeleteTextures(1, &m_textureID);
}

void Texture::bind(unsigned int slot){
    m_slot = slot;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
}

void Texture::unbind()const {
    glActiveTexture(GL_TEXTURE0 + m_slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::shared_ptr<Texture> Texture::getFallbackWhite() {
    static std::shared_ptr<Texture> whiteFallback = nullptr;

    if (!whiteFallback) {
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        whiteFallback = std::make_shared<Texture>(whitePixel, 1, 1, 4);
    }
    return whiteFallback;
}


std::shared_ptr<Texture> Texture::getFallbackBlack() {
    static std::shared_ptr<Texture> blackFallback = nullptr;

    if (!blackFallback) {
        unsigned char blackPixel[4] = { 0, 0, 0, 255 };
        blackFallback = std::make_shared<Texture>(blackPixel, 1, 1, 4);
    }
    return blackFallback;
}
