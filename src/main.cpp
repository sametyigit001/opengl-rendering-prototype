#define STB_IMAGE_IMPLEMENTATION
#include "Window.h"
#include "Shader.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "vertexArray.h"
#include "vertexBuffer.h"
#include "elementBuffer.h"
#include "Camera.h"
#include "Texture.h"
#include <iostream>

Camera* active_cam = nullptr;
void mouse_callback_handler(GLFWwindow* window, double xPos, double yPos);

int main()
{
    stbi_set_flip_vertically_on_load(true);
    Window window(800, 600, "test");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    if (!window.isValid()) {
        std::cout << "initializing error" << std::endl;
        return 1;
    }

    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    Shader shaders("C:/Users/Samet/Documents/shaders/shader.vert", "C:/Users/Samet/Documents/shaders/shader.frag");
    Shader lightShders("C:/Users/Samet/Documents/shaders/light.vert", "C:/Users/Samet/Documents/shaders/light.frag");

    cameraConfig config;
    config.type = projectionType::perspective;
    config.windowHeight = window.getHeight();
    config.windowWidth = window.getWidth();
    config.farPlane = 800.0f;
    config.nearPlane = 0.1f;
    Camera camera(config);
    active_cam = &camera;
    glfwSetCursorPosCallback(window.getWindow(), mouse_callback_handler);

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    VBO vbo(sizeof(vertices), vertices);
    VAO vao;
    VAO lightVAO;
    EBO ebo(static_cast<unsigned int>(sizeof(indices)), indices);

    Texture diffuseMap("C:/Users/Samet/Downloads/c.png", texType::tex2d);
    Texture specularMap("C:/Users/Samet/Downloads/d.png", texType::tex2d);

    vao.build(vbo, { 3, 3, 2 });
    lightVAO.build(vbo, { 3, 3, 2 });

    // World positions for point lights (Index 0 is set dynamically to camera position)
    glm::vec3 pointLightPositions[] = {
        glm::vec3(0.0f, 0.0f, 0.0f), // Updated in loop to camera.pos
        glm::vec3(1.2f, 5.0f, 2.0f),
        glm::vec3(-2.3f, 3.3f, -4.0f),
        glm::vec3(0.0f, 2.0f, -3.0f)
    };

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

        // Update Point Light 0 position to mirror camera
        pointLightPositions[0] = camera.pos;

        // --- 1. RENDER OBJECT WITH MAIN SHADER ---
        shaders.use();

        // Material setup
        shaders.setInt("material.diffuse", 0);
        shaders.setInt("material.specular", 1);
        shaders.setFloat("material.shininess", 32.0f);

        // Position & Direction (reading via your public references)
        shaders.setVec3("L_spot.position", camera.pos);
        shaders.setVec3("L_spot.direction", camera.front);
        shaders.setVec3("L_dir.direction", glm::vec3(3.0f,-4.0f,2.0f));

        // Cutoff angles (converted to cosine)
        shaders.setFloat("L_spot.cutOff", glm::cos(glm::radians(12.5f)));
        shaders.setFloat("L_spot.outerCutOff", glm::cos(glm::radians(17.5f)));

        // Colors
        shaders.setVec3("L_spot.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
        shaders.setVec3("L_spot.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));
        shaders.setVec3("L_spot.specular", glm::vec3(1.0f, 1.0f, 1.0f));

        shaders.setVec3("L_dir.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
        shaders.setVec3("L_dir.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));
        shaders.setVec3("L_dir.specular", glm::vec3(1.0f, 1.0f, 1.0f));

        // Attenuation (~50m range)
        shaders.setFloat("L_spot.constant", 1.0f);
        shaders.setFloat("L_spot.linear", 0.09f);
        shaders.setFloat("L_spot.quadratic", 0.032f);

        // Camera Uniforms & Textures
        shaders.setVec3("viewPos", camera.GetPosition());
        diffuseMap.bind(0);
        specularMap.bind(1);

        // Object Model Matrix
        glm::mat4 model = glm::mat4(1.0f);
        shaders.setUniformProjection(camera.getProjectionMatrix());
        shaders.setUniformView(camera.getViewMatrix());
        shaders.setUniformModel(model);

        vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        vao.unbind();

        // --- 2. RENDER LIGHT CUBES FOR POINT LIGHTS ---
        lightShders.use();
        lightVAO.bind();

        lightShders.setUniformProjection(camera.getProjectionMatrix());
        lightShders.setUniformView(camera.getViewMatrix());

     
        // --- FPS COUNTER ---
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

    glfwTerminate();
    return 0;
}

void mouse_callback_handler(GLFWwindow* window, double xPos, double yPos) {
    if (active_cam != nullptr) {
        active_cam->mouse_callback(window, xPos, yPos);
    }
}