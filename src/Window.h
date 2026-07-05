#ifndef WINDOW_H
#define WINDOW_H
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <iostream>
#include <string>

class Window {
private:
	GLFWwindow* m_window;
	int m_width;
	int m_height;
	std::string m_title;
	bool m_IsInitiailized;
public:
	Window(int width, int height, std::string title);
	void swapbuffer();
	void pollevents();
	bool isValid();
	bool shouldClose() { return glfwWindowShouldClose(m_window); }
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
	void onResize(int width, int height);
	int getHeight() const { return m_height; };
	int getWidth() const { return m_width; };
	~Window();
	GLFWwindow* getWindow() { return m_window; }

};


#endif 	