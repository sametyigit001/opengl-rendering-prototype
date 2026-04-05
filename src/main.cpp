#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "test", NULL, NULL);
    if (window == NULL) {
                std::cout<<"Failed to create GLFW window"<<std::endl;
        glfwTerminate();
		return -1;
    }
	glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);


    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cout<<"Failed to initialize GLAD"<<std::endl;
				return -1;
    }
	glViewport(0, 0, 800, 600);
	
    float vertices[]{
      -1.0f,0.0f,0.0f,
      -0.5f,1.0f,0.0f,
      0.0f,0.0f,0.0f,

       0.0f,0.0f,0.0f,
       0.5f,1.0f,0.0f,
       1.0f,0.0f,0.0f,
    };
   

    const char* vertexShaderSource = "#version 330 core \n"
        "layout (location = 0) in vec3 aPos;\n"
        "void main()\n"
        "{\n"
        "gl_Position = vec4(aPos.x,aPos.y,aPos.z, 1.0);\n"
        "}\0";

    const char* fragShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "FragColor =vec4(1.0f,0.5f,0.2f,1.0f);\n"
        "}\0";

    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int FragmentShader;
    FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(FragmentShader, 1, &fragShaderSource, NULL);
    glCompileShader(FragmentShader);

    int success,success2;
    char infolog[512],infolog2[512];
    glGetShaderiv(FragmentShader,GL_COMPILE_STATUS,&success2);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!(success))
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infolog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infolog << std::endl;
    }
    if (!(success2))
    {
        glGetShaderInfoLog(FragmentShader, 512, NULL, infolog2);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infolog2 << std::endl;
    }




    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, FragmentShader);
    glLinkProgram(shaderProgram);
    int success3;
    char infolog3[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success3);
    if (!success3) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infolog3);
        std::cout << "error on program: " <<  infolog3  << std::endl;
    }
    glUseProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(FragmentShader);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);


    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    while (!glfwWindowShouldClose(window)) {
	    processInput(window);

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
		glfwPollEvents();
    }
	glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

