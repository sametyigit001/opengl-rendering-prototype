#include "Texture.h"

Texture::Texture(const char* path, texType type, const textureConfig& config){
	m_type = type;
	glGenTextures(1, &m_textureID);
	unsigned char* data = stbi_load(path, &m_width, &m_height,&m_channels,0);
	switch (m_type) {

		case(texType::tex2d): {
			glBindTexture(GL_TEXTURE_2D, m_textureID);
			switch (config.wrapS) {
			case(wrapMode::repeat): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
			case(wrapMode::clamp): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
			case(wrapMode::border): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); break;
			case(wrapMode::mirror): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT); break;
			}
			switch (config.wrapT) {
			case(wrapMode::repeat): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
			case(wrapMode::clamp): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
			case(wrapMode::border): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER); break;
			case(wrapMode::mirror): glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT); break;

			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if (data) {
				GLenum format = GL_RGB;
				if (m_channels == 1)      format = GL_RED;
				else if (m_channels == 3) format = GL_RGB;
				else if (m_channels == 4) format = GL_RGBA;

				glTexImage2D(GL_TEXTURE_2D,0,format,m_width,m_height,0,format,GL_UNSIGNED_BYTE,data);
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			else {
				std::cout << "failed to load texture" << std::endl;
			}
			stbi_image_free(data);
	    }
	}
}
Texture::Texture(const char* path, texType type) : Texture(path, type, textureConfig{wrapMode::repeat,wrapMode::repeat}) {}
Texture::~Texture() {
	glDeleteTextures(1,&m_textureID);
}
void Texture::bind(unsigned int slot) {
	glActiveTexture(GL_TEXTURE0 + slot);
	m_slot = slot;
	glBindTexture(GL_TEXTURE_2D, m_textureID);
}
void Texture::unbind() {
	glActiveTexture(GL_TEXTURE0 + m_slot);
	glBindTexture(GL_TEXTURE_2D, 0);
}