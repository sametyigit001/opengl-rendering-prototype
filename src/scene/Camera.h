#ifndef CAMERA_H
#define CAMERA_H

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <cmath>

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
    float nearPlane = 0.050f;
    float farPlane = 1000.0f;
    float cameraSpeed = 50.0f;
};

class Camera {
private:
    projectionType m_Type;
    float m_Width, m_Height, m_Near, m_Far, m_FOV;
    glm::vec3 localCameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 localCameraRotation = glm::vec3(0.0f);
    glm::vec3 localCameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 localCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 cameraPos = glm::vec3(0.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float m_camSpeed;

public:
    double lastX = 400, lastY = 300;
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool firstMouse = true;
    float ambient = 0.5f;

    const glm::vec3& getPos() const { return cameraPos; }
    const glm::vec3& getLocalPos() const { return localCameraPos; }
    const glm::vec3& getLocalRotation() const { return localCameraRotation; }
    const glm::vec3& getFront() const { return cameraFront; }
    const glm::vec3& getUp() const { return cameraUp; }
    void setCameraPos(glm::vec3 pos) {
        localCameraPos = pos;
    }
    void setCameraRotation(glm::vec3 rotation) {
        localCameraRotation = rotation;
        glm::mat4 matrix(1.0f);
        matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        localCameraFront = glm::mat3(matrix) * glm::vec3(0, 0, -1);
        localCameraUp = glm::mat3(matrix) * glm::vec3(0, 1, 0);
    }
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
    void setFOV(float fov) {
        m_FOV = fov;
    }

    float getSpeed() {
        return m_camSpeed;
    }
    float getNearPlane() {
        return m_Near;
    }
    float getFarPlane() {
        return m_Far;
    }
    float getFOV() {
        return m_FOV;
    }

    void moveForward(float deltatime) {
        localCameraPos += cameraFront * m_camSpeed * deltatime;
    }
    void moveBackward(float deltatime) {
        localCameraPos -= cameraFront * m_camSpeed * deltatime;
    }
    void moveRight(float deltatime) {
        localCameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * m_camSpeed * deltatime;
    }
    void moveLeft(float deltatime) {
        localCameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * m_camSpeed * deltatime;
    }
    void moveUp(float deltatime) {
        localCameraPos += cameraUp * m_camSpeed * deltatime;
    }
    void moveDown(float deltatime) {
        localCameraPos -= cameraUp * m_camSpeed * deltatime;
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
        localCameraFront = glm::normalize(direction);

        localCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    void setAspect(float width, float height) {
        if (height <= 0.0f) height = 1.0f;
        if (width <= 0.0f) width = 1.0f;
        m_Width = width;
        m_Height = height;
    }
    void syncToWorldMatrix(const glm::mat4& worldMatrix) {
        cameraPos = glm::vec3(worldMatrix * glm::vec4(localCameraPos, 1.0f));
        // Scale affects the offset, but the view needs an orthonormal basis.
        glm::vec3 right(worldMatrix[0]);
        glm::vec3 up(worldMatrix[1]);
        if (glm::dot(right, right) < 0.000001f) return;
        right = glm::normalize(right);
        up -= right * glm::dot(right, up);
        if (glm::dot(up, up) < 0.000001f) return;
        up = glm::normalize(up);
        glm::mat3 rotation(right, up, glm::cross(right, up));
        cameraFront = glm::normalize(rotation * localCameraFront);
        cameraUp = glm::normalize(rotation * localCameraUp);
    }
};

#endif