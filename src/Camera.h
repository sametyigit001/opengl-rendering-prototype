#ifndef CAMERA_H
#define CAMERA_H
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "Window.h"

enum class projectionType {
	perspective,
	orthographic
};

struct cameraConfig {
	projectionType type = projectionType::perspective;

	float windowWidth = 800.0f;
	float windowHeight = 600.0f;
	float FOV = 45.0f;
	float nearPlane = -1000.0f;
	float farPlane = 1000.0f;
	float cameraSpeed = 3.0f;
};

class Camera {
private:

	projectionType m_Type;
	float m_Width, m_Height, m_Near, m_Far, m_FOV;
	GLFWwindow* m_windowCamera = nullptr;
	glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	float m_camSpeed;
	
public:
	float lastX = 400, lastY = 300;
	float yaw = -89.0f;
	float pitch = 0.0f;
	bool firstMouse = true;


	explicit Camera(const cameraConfig& config) {
		m_Type = config.type;
		m_Width = config.windowWidth;
		m_Height = config.windowHeight;
		m_FOV = config.FOV;
		m_Far = config.farPlane;
		m_Near = config.nearPlane;
		m_camSpeed = config.cameraSpeed;

	
	};
	glm::mat4 getProjectionMatrix() {
		switch (m_Type) {
		case projectionType::perspective: {
			float safeNear = (m_Near <= 0.0f) ? 0.1f : m_Near;
			return glm::perspective(glm::radians(m_FOV), m_Width / m_Height, safeNear, m_Far);
		}

		case projectionType::orthographic:
			return glm::ortho(0.0f, m_Width, 0.0f, m_Height, m_Near, m_Far);
		}
		return glm::mat4(1.0f);
	}

	glm::mat4 getViewMatrix() {
		return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	}


	void processInput(Window* window, float deltatime) {
		GLFWwindow* win = window->getWindow();
		const float speedMultiplier = m_camSpeed;
		const float speed = deltatime * speedMultiplier;
		if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
			cameraPos += speed * cameraFront;
		if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
			cameraPos -= speed * cameraFront;
		if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
			cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
		if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
			cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
		if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(win, GLFW_TRUE);
		}
	};
	

	void mouse_callback(GLFWwindow* window,double xpos,double ypos){
		
		
		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos;
		lastX = xpos;
		lastY = ypos;

		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		glm::vec3 direction;
		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(direction);
	}
	void setAspect(float width,float height){
		m_Height = height;
		m_Width = width;
	}
};
#endif
