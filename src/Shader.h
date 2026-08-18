#ifndef SHADER_H
#define SHADER_H
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Lights.h"
#include <fstream>
#include <sstream>
#include <iostream>
class Shader {
private:
	unsigned int m_ProgramID;
	mutable std::unordered_map<std::string, int> m_uniformCache;
	int getUniformLocation(const std::string& name)const;
public:
	Shader(const char* vertexPath, const char* fragmentPath);
	~Shader();
	void use();
	void setUniformModel(const glm::mat4& data) const;
	void setUniformView(const glm::mat4& data) const;
	void setUniformProjection(const glm::mat4& data) const;
	void setVec3(const char* name ,const glm::vec3& data) const;
	void setInt(const char* name,int data) const;
	void setFloat(const char* name,float data) const;
	void setLight(const lightConfig& lightData, int index)const ;

};

#endif