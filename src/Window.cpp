	#include "Window.h"

	Window::Window(int width, int height, std::string title) {
		m_window = nullptr;
		m_height = height;
		m_width = width;
		m_title = title;


		if (!glfwInit()) {
			std::cout << "error initializing glfw" << std::endl;
			m_IsInitiailized = false;
			return;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
			m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), NULL, NULL);
			if (m_window == NULL) {
				std::cout << "error during window creation" << std::endl;
				m_IsInitiailized = false;
				return;
			}
			glfwMakeContextCurrent(m_window);
			glfwSwapInterval(1);

			glfwSetWindowUserPointer(m_window,this);
			glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
			glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
				std::cout << "error during glad initializing" << std::endl;
				m_IsInitiailized = false;
				return;
			}
			m_IsInitiailized = true;
	
	}
	Window::~Window() {
		if (m_window != nullptr) {
			glfwDestroyWindow(m_window);

		}
		};
	void Window::swapbuffer() {
		glfwSwapBuffers(m_window);
	};
	void Window::pollevents() {
		glfwPollEvents();
	};
	bool Window::isValid() {
		if (!m_IsInitiailized) {
			return false;
		}
		else {
			return true;
		}

	};
	   void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
		   Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
		   if (w != nullptr) {
			   w->onResize(width,height);
		   }
	};
	   void Window::onResize(int width, int height) {
		   m_width = width;
		   m_height = height;
		   glViewport(0, 0, width, height);

	   }

	   
   