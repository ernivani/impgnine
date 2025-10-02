#pragma once

#include "../backend/window.hpp"
#include "../camera.hpp"
#include "../ui/UIRenderer.hpp"
#include "../ui/UIWindow.hpp"
#include <vector>

namespace impgine {

class InputManager {
public:
    InputManager(Window* window, Camera* camera);

    void processInput(float deltaTime);
    void handleMouseMovement();
    void handleUIInput(UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows);

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods, UIRenderer* uiRenderer, std::vector<UIWindow>& uiWindows);

    bool isMouseCaptured() const { return mouseCaptured; }
    void setMouseCaptured(bool captured) { mouseCaptured = captured; }

private:
    Window* window;
    Camera* camera;

    double lastMouseX;
    double lastMouseY;
    bool firstMouse = true;
    bool mouseCaptured = true;
};

} // namespace impgine
