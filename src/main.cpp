#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) return -1;

    // 2. Create Window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Engine Prototype", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 3. Initialize GLAD (This is where glad.c does its job!)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 4. Render Loop
    while (!glfwWindowShouldClose(window)) {
        // Render a Dark Teal background
        glClearColor(0.1f, 0.4f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}