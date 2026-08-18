#include "Shader.h"

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vertexFile;
	std::ifstream fragmentFile;

	vertexFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fragmentFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try {
		fragmentFile.open(fragmentPath);
		vertexFile.open(vertexPath);
		std::stringstream vertexStream, fragmentStream;

		vertexStream << vertexFile.rdbuf();
		fragmentStream << fragmentFile.rdbuf();

		fragmentFile.close();
		vertexFile.close();
		vertexCode = vertexStream.str();
		fragmentCode = fragmentStream.str();
	}
	catch (std::ifstream::failure e) {
		std::cout << "error reading shader files" << std::endl;
		return;
	}
		const char* vShadercode = vertexCode.c_str();
		const char* fShadercode = fragmentCode.c_str();
		unsigned int vertex, fragment;
		int success;
		char infolog[512];
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShadercode, NULL);
		glCompileShader(vertex);
		glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vertex, 512, NULL, infolog);
			std::cout << "error compiling vertex shader" << infolog << std::endl;
		}
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShadercode, NULL);
		glCompileShader(fragment);
		glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(fragment, 512, NULL, infolog);
			std::cout << "error compiling fragment shader" << infolog << std::endl;
		}
		m_ProgramID = glCreateProgram();
		glAttachShader(m_ProgramID, vertex);
		glAttachShader(m_ProgramID, fragment);
		glLinkProgram(m_ProgramID);
		glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(m_ProgramID, 512, NULL, infolog);
			std::cout << "error linking program" << infolog << std::endl;
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
}

int Shader::getUniformLocation(const std::string& name)const {
	auto it = m_uniformCache.find(name);
	if (it != m_uniformCache.end()) {
		return it->second;
	}
	int location = glGetUniformLocation(m_ProgramID, name.c_str());
	if (location == -1) {
		m_uniformCache[name] = location;
	}
	m_uniformCache[name] = location;
	return location;
}

Shader::~Shader() {
	if (m_ProgramID != 0) {
		glDeleteProgram(m_ProgramID);
	}
}
void Shader::use() {
	glUseProgram(m_ProgramID);
}

void Shader::setUniformModel(const glm::mat4& data) const{


	glUniformMatrix4fv(getUniformLocation("uModel"), 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setUniformView(const glm::mat4& data) const{


	glUniformMatrix4fv(getUniformLocation("uView"), 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setUniformProjection(const glm::mat4& data) const{

	glUniformMatrix4fv(getUniformLocation("uProjection"), 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setInt(const char* name, int data)const {
	glUniform1i(getUniformLocation(name), data);
}
void Shader::setFloat(const char* name, float data)const {
	glUniform1f(getUniformLocation(name), data);
}
void Shader::setVec3(const char* name, const glm::vec3& data)const {
	glUniform3fv(getUniformLocation(name),1, glm::value_ptr(data));
}


void Shader::setLight( const lightConfig& lightData,int index)const {

	std::string prefix = "light[" + std::to_string(index) + "].";

	setInt((prefix + "type").c_str(), static_cast<int>(lightData.type));

	setVec3((prefix + "direction").c_str(), lightData.direction);
	setVec3((prefix + "position").c_str(), lightData.position);
	setVec3((prefix + "ambient").c_str(), lightData.ambient);
	setVec3((prefix + "diffuse").c_str(), lightData.diffuse);
	setVec3((prefix + "specular").c_str(), lightData.specular);

	setFloat((prefix + "constant").c_str(),lightData.constant);
	setFloat((prefix + "linear").c_str(),lightData.linear);
	setFloat((prefix + "quadratic").c_str(),lightData.quadratic);
	setFloat((prefix + "cutOff").c_str(),lightData.cutOff);
	setFloat((prefix + "outerCutOff").c_str(),lightData.outerCutOff);

}
