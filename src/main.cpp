#include "Window.h"
#include "Shader.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "Camera.h"
#include "Lights.h"
#include <iostream>
#include "Model.h"


Camera* active_cam = nullptr;
void mouse_callback_handler(GLFWwindow* window, double xPos, double yPos);

int main()
{
    Window window(800, 600, "test");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (!window.isValid()) {
        std::cout << "initializing error" << std::endl;
        return 1;
    }

    {
        Shader shaders("src/shaders/shader.vert", "src/shaders/shader.frag");

        cameraConfig configCamera;
        configCamera.type = projectionType::perspective;
        configCamera.windowHeight = window.getHeight();
        configCamera.windowWidth = window.getWidth();
        configCamera.farPlane = 800.0f;
        configCamera.nearPlane = 0.1f;
        Camera camera(configCamera);
        active_cam = &camera;
        glfwSetCursorPosCallback(window.getWindow(), mouse_callback_handler);

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        lightManager lightMan;
        lightConfig config;
        config.type = lightTypes::directional;
        lightMan.addLights(config);

        Model backpack("assets/test3/source/lucy.ply");

        glfwSwapInterval(1);

        while (!window.shouldClose()) {
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            float windowHeight = static_cast<float>(window.getHeight());
            float windowWidth = static_cast<float>(window.getWidth());

            camera.setAspect(windowWidth, windowHeight);
            camera.setSpeed(5);
            camera.processInput(&window, deltaTime);

            shaders.use();

            shaders.setFloat("material.shininess", 32.0f);
            shaders.setInt("usedLight", lightMan.getSize());

            lightMan.applyLights(shaders);

            shaders.setVec3("viewPos", camera.GetPosition());

            glm::mat4 model = glm::mat4(1.0f);
            shaders.setUniformProjection(camera.getProjectionMatrix());
            shaders.setUniformView(camera.getViewMatrix());
            model = glm::translate(model, glm::vec3(0.0f, -2.0f, -5.0f));
            model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f,0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.01f));
            shaders.setUniformModel(model);

            backpack.draw(shaders);

            static double lastTime = glfwGetTime();
            static int nbFrames = 0;
            double currentTime = glfwGetTime();
            nbFrames++;
            if (currentTime - lastTime >= 1.0) {
                std::string title = "OpenGL - FPS: " + std::to_string(nbFrames);
                glfwSetWindowTitle(window.getWindow(), title.c_str());
                nbFrames = 0;
                lastTime += 1.0;
            }

            window.swapbuffer();
            window.pollevents();
        }

    }
    glfwTerminate();
    return 0;
}

void mouse_callback_handler(GLFWwindow* window, double xPos, double yPos) {
    if (active_cam != nullptr) {
        active_cam->mouse_callback(window, xPos, yPos);
    }
}
