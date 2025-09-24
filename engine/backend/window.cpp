#include "window.hpp"

#include <iostream>
#include <stdexcept>

namespace impgine {

    Window::Window(int width, int height,
        const std::string & title): width(width), height(height), windowTitle(title) {
        initWindow();
    }

    Window::~Window() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, windowTitle.c_str(), nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(window);
    }

    void Window::pollEvents() const {
        glfwPollEvents();
    }

    void Window::waitEvents() const {
        glfwWaitEvents();
    }

    VkExtent2D Window::getExtent() const {
        return {
            static_cast < uint32_t > (width),
            static_cast < uint32_t > (height)
        };
    }

    void Window::getFramebufferSize(int * width, int * height) const {
        glfwGetFramebufferSize(window, width, height);
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR * surface) const {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
    }

    void Window::setUserPointer(void * pointer) {
        glfwSetWindowUserPointer(window, pointer);
    }

    void Window::setFramebufferSizeCallback(GLFWframebuffersizefun callback) {
        glfwSetFramebufferSizeCallback(window, callback);
    }

} // namespace impgine