#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <string>

namespace impgine {

    class Window {
        public: Window(int width, int height,
            const std::string & title);
        ~Window();

        // Delete copy constructor and assignment operator
        Window(const Window & ) = delete;
        Window & operator = (const Window & ) = delete;

        bool shouldClose() const;
        void pollEvents() const;
        void waitEvents() const;

        VkExtent2D getExtent() const;
        void getFramebufferSize(int * width, int * height) const;

        void createWindowSurface(VkInstance instance, VkSurfaceKHR * surface) const;

        GLFWwindow * getGLFWWindow() const {
            return window;
        }

        void setUserPointer(void * pointer);
        void setFramebufferSizeCallback(GLFWframebuffersizefun callback);
        
        // Input handling
        bool isKeyPressed(int key) const;
        void getCursorPos(double* xpos, double* ypos) const;
        void setCursorPos(double xpos, double ypos) const;
        void setCursorInputMode(int mode) const;

        private: int width;
        int height;
        std::string windowTitle;

        GLFWwindow * window;

        void initWindow();
    };

} // namespace impgine