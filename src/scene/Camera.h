#ifndef CAMERA_H
#define CAMERA_H

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glad/glad.h"
#include "render/Shader.h"

enum class projectionType {
    perspective,
    orthographic
};

enum class cameraMovement : int {
    forward = 0,
    backward = 1,
    left = 2,
    right = 3,
    up = 4,
    down = 5
};

struct cameraConfig {
    projectionType type = projectionType::perspective;

    float windowWidth = 800.0f;
    float windowHeight = 600.0f;
    float FOV = 45.0f;
    float nearPlane = 0.1f;       
    float farPlane = 20000.0f;    
    float cameraSpeed = 50.0f;
};

class Camera {
private:
    projectionType m_Type;
    float m_Width, m_Height, m_Near, m_Far, m_FOV;
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float m_camSpeed;

public:
    double lastX = 400, lastY = 300;
    float yaw = -89.0f;
    float pitch = 0.0f;
    bool firstMouse = true;
    float ambient = 0.5f;

    const glm::vec3 getPos() const { return cameraPos; }
    const glm::vec3& getFront() const { return cameraFront; }
    const glm::vec3& getUp() const { return cameraUp; }

    explicit Camera(const cameraConfig& config) {
        m_Type = config.type;
        m_Width = config.windowWidth;
        m_Height = config.windowHeight;
        m_FOV = config.FOV;
        m_Far = config.farPlane;
        m_Near = config.nearPlane;
        m_camSpeed = config.cameraSpeed;
    }

    glm::mat4 getProjectionMatrix() {
        switch (m_Type) {
        case projectionType::perspective: {
            float aspect = (m_Height > 0.0f) ? (m_Width / m_Height) : (16.0f / 9.0f);
            float safeNear = (m_Near <= 0.0f) ? 0.1f : m_Near;
            return glm::perspective(glm::radians(m_FOV), aspect, safeNear, m_Far);
        }
        case projectionType::orthographic:
            return glm::ortho(0.0f, m_Width, 0.0f, m_Height, m_Near, m_Far);
        }
        return glm::mat4(1.0f);
    }

    glm::mat4 getViewMatrix() {
        return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }

    void setSpeed(float val) {
        m_camSpeed = val;
    }

    void setFarPlane(float farPlane) {
        m_Far = farPlane;
    }

    void setNearPlane(float nearPlane) {
        m_Near = nearPlane;
    }

    void moveForward(float deltatime) {
        cameraPos += cameraFront * m_camSpeed * deltatime;
    }
    void moveBackward(float deltatime) {
        cameraPos -= cameraFront * m_camSpeed * deltatime;
    }
    void moveRight(float deltatime) {
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * m_camSpeed * deltatime;
    }
    void moveLeft(float deltatime) {
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * m_camSpeed * deltatime;
    }
    void moveUp(float deltatime) {
        cameraPos += cameraUp * m_camSpeed * deltatime;
    }
    void moveDown(float deltatime) {
        cameraPos -= cameraUp * m_camSpeed * deltatime;
    }

    const glm::vec3& GetPosition() const { return cameraPos; }

    void mouse_callback(float xoffset, float yoffset, bool constrainPitch = true) {
        const float sens = 0.10f;
        xoffset *= sens;
        yoffset *= sens;

        yaw += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }

        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(direction);

        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    void setAspect(float width, float height) {
        if (height <= 0.0f) height = 1.0f;
        if (width <= 0.0f) width = 1.0f;
        m_Width = width;
        m_Height = height;
    }
};

#endif
