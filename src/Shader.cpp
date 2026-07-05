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
Shader::~Shader() {
	if (m_ProgramID != 0) {
		glDeleteProgram(m_ProgramID);
	}
}
void Shader::use() {
	glUseProgram(m_ProgramID);
}

void Shader::setUniformModel(const glm::mat4& data) const{

	unsigned int location = glGetUniformLocation(m_ProgramID, "uModel");

	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setUniformView(const glm::mat4& data) const{

	unsigned int location = glGetUniformLocation(m_ProgramID, "uView");

	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setUniformProjection(const glm::mat4& data) const{

	unsigned int location = glGetUniformLocation(m_ProgramID, "uProjection");

	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(data));
}

void Shader::setInt(const char* name, int slot) {
	unsigned int location = glGetUniformLocation(m_ProgramID, name);

	glUniform1i(location, slot);
}
