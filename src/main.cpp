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
	if (!window.isValid()) {
		std::cout << "initializing error" << std::endl;
		return 1;
	}
    float vertices[] = {
        // positions          // colors           // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left 
    };
    unsigned int indices[] = {
        0,1,3,
        1,2,3
    };

	Shader shaders("C:/Users/Samet/Documents/shaders/shader.vert","C:/Users/Samet/Documents/shaders/shader.frag");
	
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

	VBO vbo(sizeof(vertices),vertices);
	VAO vao;
    EBO ebo(static_cast<unsigned int>(sizeof(indices)),indices);
    Texture textureWall("C:/Users/Samet/Downloads/a.jpg", texType::tex2d);
    Texture textureSmile("C:/Users/Samet/Downloads/b.png", texType::tex2d);
	vao.build(vbo,ebo,{3,3,2});
    
 
    glfwSwapInterval(1);
	while(!window.shouldClose()){
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        shaders.use();
        shaders.setInt("tex0", 0);
        shaders.setInt("tex1", 1);
        textureWall.bind(0);
        textureSmile.bind(1);

        glm::mat4 model = glm::mat4(1.0f);
        float windowHeight = static_cast<float>(window.getHeight());
        float windowWidth = static_cast<float>(window.getWidth());

        camera.setAspect(windowWidth, windowHeight);
        camera.processInput(&window, deltaTime);

        shaders.setUniformProjection(camera.getProjectionMatrix());
        shaders.setUniformView(camera.getViewMatrix());
        shaders.setUniformModel(model);
        


		vao.bind();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		vao.unbind();


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