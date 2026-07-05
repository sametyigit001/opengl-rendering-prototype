#ifndef TEXTURE_H
#define TEXTURE_H


#include "STB/stb_image.h"
#include "glad/glad.h" 
#include "GLFW/glfw3.h" 
#include <iostream>

enum class texType{
	tex2d,
	tex3d
};
enum class wrapMode
{
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
	Texture(const char* texturePath, texType type);
	Texture(const char* texturePath, texType type,const textureConfig& config);
	~Texture();
	void bind(unsigned int slot = 0);
	void unbind();
};

#endif
